#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIds.h"

namespace folkpark
{
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "FolkParkState", createParameterLayout())
{
    masterGainParameter = parameters.getRawParameterValue(parameterIds::masterGain);
    waveformParameter = parameters.getRawParameterValue(parameterIds::oscillatorWaveform);
    oscillatorLevelParameter = parameters.getRawParameterValue(parameterIds::oscillatorLevel);
    subLevelParameter = parameters.getRawParameterValue(parameterIds::subLevel);
    cutoffParameter = parameters.getRawParameterValue(parameterIds::filterCutoff);
    attackParameter = parameters.getRawParameterValue(parameterIds::ampAttack);
    decayParameter = parameters.getRawParameterValue(parameterIds::ampDecay);
    sustainParameter = parameters.getRawParameterValue(parameterIds::ampSustain);
    releaseParameter = parameters.getRawParameterValue(parameterIds::ampRelease);

    jassert(masterGainParameter != nullptr && waveformParameter != nullptr
            && oscillatorLevelParameter != nullptr && subLevelParameter != nullptr
            && cutoffParameter != nullptr && attackParameter != nullptr
            && decayParameter != nullptr && sustainParameter != nullptr
            && releaseParameter != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::masterGain, 1}, "Master Gain",
        juce::NormalisableRange<float>{-60.0f, 6.0f, 0.01f}, -12.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{parameterIds::oscillatorWaveform, 1}, "Oscillator Waveform",
        juce::StringArray{"Sine", "Triangle"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::oscillatorLevel, 1}, "Oscillator Level",
        juce::NormalisableRange<float>{-60.0f, 0.0f, 0.01f}, -6.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::subLevel, 1}, "Sub Level",
        juce::NormalisableRange<float>{-60.0f, 0.0f, 0.01f}, -18.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::filterCutoff, 1}, "Filter Cutoff",
        juce::NormalisableRange<float>{20.0f, 20000.0f, 1.0f, 0.25f}, 12000.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::ampAttack, 1}, "Amp Attack",
        juce::NormalisableRange<float>{0.001f, 5.0f, 0.001f, 0.35f}, 0.01f,
        juce::AudioParameterFloatAttributes{}.withLabel("s")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::ampDecay, 1}, "Amp Decay",
        juce::NormalisableRange<float>{0.001f, 5.0f, 0.001f, 0.35f}, 0.15f,
        juce::AudioParameterFloatAttributes{}.withLabel("s")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::ampSustain, 1}, "Amp Sustain",
        juce::NormalisableRange<float>{0.0f, 1.0f, 0.001f}, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterIds::ampRelease, 1}, "Amp Release",
        juce::NormalisableRange<float>{0.005f, 10.0f, 0.001f, 0.3f}, 0.4f,
        juce::AudioParameterFloatAttributes{}.withLabel("s")));
    return layout;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    activeSampleRate = sampleRate;
    activeBlockSize = maximumExpectedSamplesPerBlock;
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    masterGain.reset(sampleRate, 0.02);
    const auto initialGain = juce::Decibels::decibelsToGain(masterGainParameter->load(std::memory_order_relaxed), -60.0f);
    masterGain.setCurrentAndTargetValue(initialGain);
}

void PluginProcessor::releaseResources()
{
    activeSampleRate = 0.0;
    activeBlockSize = 0;
    engine.reset();
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(activeSampleRate, activeBlockSize);

    if (panicRequested.exchange(false, std::memory_order_acq_rel))
        engine.panic();

    engine.process(audio, midi, readSynthParameters());
    masterGain.setTargetValue(juce::Decibels::decibelsToGain(
        masterGainParameter->load(std::memory_order_relaxed), -60.0f));

    const auto channelCount = juce::jmin(2, audio.getNumChannels());
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto gain = masterGain.getNextValue();
        for (int channel = 0; channel < channelCount; ++channel)
            audio.setSample(channel, sample, audio.getSample(channel, sample) * gain);
    }
}

synth::ParameterSnapshot PluginProcessor::readSynthParameters() const noexcept
{
    synth::ParameterSnapshot snapshot;
    snapshot.waveform = juce::roundToInt(waveformParameter->load(std::memory_order_relaxed));
    snapshot.oscillatorLevelDb = oscillatorLevelParameter->load(std::memory_order_relaxed);
    snapshot.subLevelDb = subLevelParameter->load(std::memory_order_relaxed);
    snapshot.filterCutoffHz = cutoffParameter->load(std::memory_order_relaxed);
    snapshot.attackSeconds = attackParameter->load(std::memory_order_relaxed);
    snapshot.decaySeconds = decayParameter->load(std::memory_order_relaxed);
    snapshot.sustainLevel = sustainParameter->load(std::memory_order_relaxed);
    snapshot.releaseSeconds = releaseParameter->load(std::memory_order_relaxed);
    return snapshot;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto snapshot = parameters.copyState();
    snapshot.setProperty("schemaVersion", 1, nullptr);
    snapshot.setProperty("productVersion", FOLK_PARK_VERSION, nullptr);
    if (const auto xml = snapshot.createXml())
        copyXmlToBinary(*xml, destination);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto candidateXml = getXmlFromBinary(data, sizeInBytes);
    if (candidateXml == nullptr)
        return;

    const auto candidate = juce::ValueTree::fromXml(*candidateXml);
    if (!candidate.isValid() || candidate.getType() != parameters.state.getType())
        return;

    const auto schemaVersion = static_cast<int>(candidate.getProperty("schemaVersion", 1));
    if (schemaVersion != 1)
        return;

    parameters.replaceState(candidate);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new folkpark::PluginProcessor();
}
