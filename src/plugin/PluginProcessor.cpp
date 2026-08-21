#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace folkpark
{
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "FolkParkState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"masterGain", 1}, "Master Gain",
        juce::NormalisableRange<float>{-60.0f, 6.0f, 0.01f}, -12.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    return layout;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    activeSampleRate = sampleRate;
    activeBlockSize = maximumExpectedSamplesPerBlock;
}

void PluginProcessor::releaseResources()
{
    activeSampleRate = 0.0;
    activeBlockSize = 0;
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi, activeSampleRate, activeBlockSize);
    audio.clear();
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
