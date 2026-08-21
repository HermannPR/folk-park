#include "PluginProcessor.h"
#include "ParameterIds.h"
#include "PluginEditor.h"

#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace folkpark
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
const juce::Identifier modulationRoutesType{"ModulationRoutes"};
const juce::Identifier modulationRouteType{"ModulationRoute"};

void addFloat(Layout& layout,
              const char* id,
              const juce::String& name,
              float minimum,
              float maximum,
              float step,
              float skew,
              float defaultValue,
              const juce::String& label = {})
{
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1}, name,
        juce::NormalisableRange<float>{minimum, maximum, step, skew}, defaultValue,
        juce::AudioParameterFloatAttributes{}.withLabel(label)));
}

void addChoice(Layout& layout,
               const char* id,
               const juce::String& name,
               const juce::StringArray& choices,
               int defaultIndex)
{
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{id, 1}, name, choices, defaultIndex));
}

void addInteger(Layout& layout,
                const char* id,
                const juce::String& name,
                int minimum,
                int maximum,
                int defaultValue)
{
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{id, 1}, name, minimum, maximum, defaultValue));
}

void addBool(Layout& layout, const char* id, const juce::String& name, bool defaultValue)
{
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{id, 1}, name, defaultValue));
}

void addEnvelope(Layout& layout,
                 const char* attackId,
                 const char* decayId,
                 const char* sustainId,
                 const char* releaseId,
                 const juce::String& prefix,
                 const synth::EnvelopeParameters& defaults)
{
    addFloat(layout, attackId, prefix + " Attack", 0.001f, 5.0f, 0.001f, 0.35f,
             defaults.attackSeconds, "s");
    addFloat(layout, decayId, prefix + " Decay", 0.001f, 5.0f, 0.001f, 0.35f,
             defaults.decaySeconds, "s");
    addFloat(layout, sustainId, prefix + " Sustain", 0.0f, 1.0f, 0.001f, 1.0f,
             defaults.sustainLevel);
    addFloat(layout, releaseId, prefix + " Release", 0.005f, 10.0f, 0.001f, 0.3f,
             defaults.releaseSeconds, "s");
}

juce::ValueTree serialiseModulationRoutes(const synth::ModulationSnapshot& snapshot)
{
    juce::ValueTree routes(modulationRoutesType);
    for (std::size_t index = 0; index < snapshot.routeCount; ++index)
    {
        const auto& source = snapshot.routes[index];
        juce::ValueTree route(modulationRouteType);
        route.setProperty("source", static_cast<int>(source.source), nullptr);
        route.setProperty("destination", static_cast<int>(source.destination), nullptr);
        route.setProperty("amount", source.amount, nullptr);
        route.setProperty("curve", static_cast<int>(source.curve), nullptr);
        route.setProperty("enabled", source.enabled, nullptr);
        routes.appendChild(route, nullptr);
    }
    return routes;
}

bool modulationSnapshotsMatch(const synth::ModulationSnapshot& first,
                              std::span<const synth::ModulationRoute> second) noexcept
{
    if (first.routeCount != second.size())
        return false;
    for (std::size_t index = 0; index < second.size(); ++index)
    {
        const auto& left = first.routes[index];
        const auto& right = second[index];
        if (left.source != right.source || left.destination != right.destination
            || std::abs(left.amount - right.amount) > std::numeric_limits<float>::epsilon()
            || left.curve != right.curve
            || left.enabled != right.enabled)
            return false;
    }
    return true;
}

bool parseInteger(const juce::var& value, int& output) noexcept
{
    if (value.isInt())
    {
        output = static_cast<int>(value);
        return true;
    }
    if (value.isInt64())
    {
        const auto parsed = static_cast<juce::int64>(value);
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
            return false;
        output = static_cast<int>(parsed);
        return true;
    }
    if (!value.isString())
        return false;
    const auto text = value.toString().trim();
    if (text.isEmpty())
        return false;
    const auto utf8 = text.toUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtol(utf8.getAddress(), &end, 10);
    if (errno != 0 || end == utf8.getAddress() || *end != '\0'
        || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
        return false;
    output = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const juce::var& value, float& output) noexcept
{
    if (value.isInt() || value.isInt64() || value.isDouble())
    {
        output = static_cast<float>(value);
        return std::isfinite(output);
    }
    if (!value.isString())
        return false;
    const auto text = value.toString().trim();
    if (text.isEmpty())
        return false;
    const auto utf8 = text.toUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtof(utf8.getAddress(), &end);
    if (errno != 0 || end == utf8.getAddress() || *end != '\0' || !std::isfinite(parsed))
        return false;
    output = parsed;
    return true;
}

bool parseBoolean(const juce::var& value, bool& output) noexcept
{
    if (value.isBool())
    {
        output = static_cast<bool>(value);
        return true;
    }
    int integer = 0;
    if (!parseInteger(value, integer) || (integer != 0 && integer != 1))
        return false;
    output = integer == 1;
    return true;
}

WavetableUiSnapshot makeWavetableUiSnapshot(const synth::WavetableBank& bank) noexcept
{
    WavetableUiSnapshot snapshot;
    snapshot.frameCount = juce::jlimit(1, WavetableUiSnapshot::maximumFrames,
                                       bank.getFrameCount());
    for (auto frame = 0; frame < snapshot.frameCount; ++frame)
    {
        for (auto sample = 0; sample < WavetableUiSnapshot::samplesPerFrame; ++sample)
        {
            const auto sourceSample = sample * synth::WavetableBank::tableSize
                / WavetableUiSnapshot::samplesPerFrame;
            snapshot.samples[static_cast<std::size_t>(
                frame * WavetableUiSnapshot::samplesPerFrame + sample)]
                = juce::jlimit(-1.0f, 1.0f, bank.getSample(frame, 0, sourceSample));
        }
    }
    return snapshot;
}

juce::Result parseModulationRoutes(const juce::ValueTree& root, synth::ModulationSnapshot& output)
{
    const auto routes = root.getChildWithName(modulationRoutesType);
    if (!routes.isValid())
    {
        output = {};
        return juce::Result::ok();
    }
    if (routes.getNumChildren() > static_cast<int>(synth::ModulationSnapshot::maximumRoutes))
        return juce::Result::fail("State contains too many modulation routes");

    std::array<synth::ModulationRoute, synth::ModulationSnapshot::maximumRoutes> parsed{};
    for (int index = 0; index < routes.getNumChildren(); ++index)
    {
        const auto route = routes.getChild(index);
        if (!route.hasType(modulationRouteType)
            || !route.hasProperty("source") || !route.hasProperty("destination")
            || !route.hasProperty("amount") || !route.hasProperty("curve")
            || !route.hasProperty("enabled"))
            return juce::Result::fail("State contains a malformed modulation route");
        const auto sourceValue = route["source"];
        const auto destinationValue = route["destination"];
        const auto amountValue = route["amount"];
        const auto curveValue = route["curve"];
        const auto enabledValue = route["enabled"];
        int sourceInteger = 0;
        int destinationInteger = 0;
        int curveInteger = 0;
        float amount = 0.0f;
        bool enabled = false;
        if (!parseInteger(sourceValue, sourceInteger)
            || !parseInteger(destinationValue, destinationInteger)
            || !parseFloat(amountValue, amount)
            || !parseInteger(curveValue, curveInteger)
            || !parseBoolean(enabledValue, enabled))
            return juce::Result::fail("State contains a modulation route with invalid field types");
        parsed[static_cast<std::size_t>(index)] = {
            static_cast<synth::ModulationSource>(sourceInteger),
            static_cast<synth::ModulationDestination>(destinationInteger),
            amount,
            static_cast<synth::ModulationCurve>(curveInteger),
            enabled,
        };
    }
    const auto parsedSpan = std::span{parsed}.first(static_cast<std::size_t>(routes.getNumChildren()));
    if (const auto validation = synth::ModulationRegistry::validate(parsedSpan); validation.failed())
        return validation;
    output = synth::ModulationRegistry::makeSnapshot(parsedSpan);
    return juce::Result::ok();
}
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "FolkParkState", createParameterLayout()),
      wavetableImportService([this](int oscillatorIndex, const synth::WavetableBank& bank)
      {
          return publishWavetable(oscillatorIndex, bank);
      })
{
    if (const auto builtIn = synth::WavetableBank::createBuiltIn())
    {
        const auto preview = makeWavetableUiSnapshot(*builtIn);
        wavetableUiSnapshots[0] = preview;
        wavetableUiSnapshots[1] = preview;
    }
    const auto raw = [this](const char* id)
    {
        auto* value = parameters.getRawParameterValue(id);
        jassert(value != nullptr);
        return value;
    };

    masterGainParameter = raw(parameterIds::masterGain);
    waveformParameter = raw(parameterIds::oscillatorWaveform);

    oscillatorAParameters.position = raw(parameterIds::oscillatorAPosition);
    oscillatorAParameters.coarse = raw(parameterIds::oscillatorACoarse);
    oscillatorAParameters.fine = raw(parameterIds::oscillatorAFine);
    oscillatorAParameters.phase = raw(parameterIds::oscillatorAPhase);
    oscillatorAParameters.randomPhase = raw(parameterIds::oscillatorARandomPhase);
    oscillatorAParameters.level = raw(parameterIds::oscillatorLevel);
    oscillatorAParameters.pan = raw(parameterIds::oscillatorAPan);
    oscillatorAParameters.unison = raw(parameterIds::oscillatorAUnison);
    oscillatorAParameters.detune = raw(parameterIds::oscillatorADetune);
    oscillatorAParameters.spread = raw(parameterIds::oscillatorASpread);
    oscillatorAParameters.blend = raw(parameterIds::oscillatorABlend);
    oscillatorAParameters.phaseReset = raw(parameterIds::oscillatorAPhaseReset);

    oscillatorBParameters.position = raw(parameterIds::oscillatorBPosition);
    oscillatorBParameters.coarse = raw(parameterIds::oscillatorBCoarse);
    oscillatorBParameters.fine = raw(parameterIds::oscillatorBFine);
    oscillatorBParameters.phase = raw(parameterIds::oscillatorBPhase);
    oscillatorBParameters.randomPhase = raw(parameterIds::oscillatorBRandomPhase);
    oscillatorBParameters.level = raw(parameterIds::oscillatorBLevel);
    oscillatorBParameters.pan = raw(parameterIds::oscillatorBPan);
    oscillatorBParameters.unison = raw(parameterIds::oscillatorBUnison);
    oscillatorBParameters.detune = raw(parameterIds::oscillatorBDetune);
    oscillatorBParameters.spread = raw(parameterIds::oscillatorBSpread);
    oscillatorBParameters.blend = raw(parameterIds::oscillatorBBlend);
    oscillatorBParameters.phaseReset = raw(parameterIds::oscillatorBPhaseReset);

    subWaveformParameter = raw(parameterIds::subWaveform);
    subOctaveParameter = raw(parameterIds::subOctave);
    subLevelParameter = raw(parameterIds::subLevel);
    noiseTypeParameter = raw(parameterIds::noiseType);
    noiseLevelParameter = raw(parameterIds::noiseLevel);
    filterModeParameter = raw(parameterIds::filterMode);
    cutoffParameter = raw(parameterIds::filterCutoff);
    filterResonanceParameter = raw(parameterIds::filterResonance);
    filterDriveParameter = raw(parameterIds::filterDrive);
    filterKeyTrackingParameter = raw(parameterIds::filterKeyTracking);
    filterEnvelopeAmountParameter = raw(parameterIds::filterEnvelopeAmount);

    ampEnvelopeParameters = {raw(parameterIds::ampAttack), raw(parameterIds::ampDecay),
                             raw(parameterIds::ampSustain), raw(parameterIds::ampRelease)};
    filterEnvelopeParameters = {raw(parameterIds::filterEnvelopeAttack), raw(parameterIds::filterEnvelopeDecay),
                                raw(parameterIds::filterEnvelopeSustain), raw(parameterIds::filterEnvelopeRelease)};
    auxiliaryEnvelopeParameters = {raw(parameterIds::auxiliaryEnvelopeAttack),
                                   raw(parameterIds::auxiliaryEnvelopeDecay),
                                   raw(parameterIds::auxiliaryEnvelopeSustain),
                                   raw(parameterIds::auxiliaryEnvelopeRelease)};
    for (std::size_t index = 0; index < lfoParameters.size(); ++index)
    {
        lfoParameters[index] = {raw(parameterIds::lfoShape[index]), raw(parameterIds::lfoRate[index]),
                                raw(parameterIds::lfoSyncDivision[index]), raw(parameterIds::lfoPhase[index]),
                                raw(parameterIds::lfoTempoSync[index]), raw(parameterIds::lfoRetrigger[index])};
    }
}

bool PluginProcessor::publishWavetable(int oscillatorIndex,
                                       const synth::WavetableBank& bank)
{
    if (oscillatorIndex < 0 || oscillatorIndex >= static_cast<int>(wavetableUiSnapshots.size())
        || !bank.isFiniteAndNormalised())
        return false;
    if (!engine.publishWavetable(oscillatorIndex, bank))
        return false;
    const auto preview = makeWavetableUiSnapshot(bank);
    const std::lock_guard lock(wavetableUiMutex);
    wavetableUiSnapshots[static_cast<std::size_t>(oscillatorIndex)] = preview;
    return true;
}

WavetableUiSnapshot PluginProcessor::getWavetableUiSnapshot(int oscillatorIndex) const
{
    const auto safeIndex = juce::jlimit(0,
        static_cast<int>(wavetableUiSnapshots.size()) - 1, oscillatorIndex);
    const std::lock_guard lock(wavetableUiMutex);
    return wavetableUiSnapshots[static_cast<std::size_t>(safeIndex)];
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    Layout layout;

    // M0/M1 compatibility surfaces remain first and unchanged.
    addFloat(layout, parameterIds::masterGain, "Master Gain", -60.0f, 6.0f, 0.01f, 1.0f, -12.0f, "dB");
    addChoice(layout, parameterIds::oscillatorWaveform, "Oscillator Waveform",
              {"Sine", "Triangle"}, 0);
    addFloat(layout, parameterIds::oscillatorLevel, "Oscillator Level",
             -60.0f, 0.0f, 0.01f, 1.0f, -6.0f, "dB");
    addFloat(layout, parameterIds::subLevel, "Sub Level",
             -60.0f, 0.0f, 0.01f, 1.0f, -18.0f, "dB");
    addFloat(layout, parameterIds::filterCutoff, "Filter Cutoff",
             20.0f, 20000.0f, 1.0f, 0.25f, 12000.0f, "Hz");
    addFloat(layout, parameterIds::ampAttack, "Amp Attack",
             0.001f, 5.0f, 0.001f, 0.35f, 0.01f, "s");
    addFloat(layout, parameterIds::ampDecay, "Amp Decay",
             0.001f, 5.0f, 0.001f, 0.35f, 0.15f, "s");
    addFloat(layout, parameterIds::ampSustain, "Amp Sustain",
             0.0f, 1.0f, 0.001f, 1.0f, 0.8f);
    addFloat(layout, parameterIds::ampRelease, "Amp Release",
             0.005f, 10.0f, 0.001f, 0.3f, 0.4f, "s");

    addFloat(layout, parameterIds::oscillatorAPosition, "Oscillator A Position",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addInteger(layout, parameterIds::oscillatorACoarse, "Oscillator A Coarse", -36, 36, 0);
    addFloat(layout, parameterIds::oscillatorAFine, "Oscillator A Fine",
             -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "cent");
    addFloat(layout, parameterIds::oscillatorAPhase, "Oscillator A Phase",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addFloat(layout, parameterIds::oscillatorARandomPhase, "Oscillator A Random Phase",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addFloat(layout, parameterIds::oscillatorAPan, "Oscillator A Pan",
             -1.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addInteger(layout, parameterIds::oscillatorAUnison, "Oscillator A Unison", 1, 8, 1);
    addFloat(layout, parameterIds::oscillatorADetune, "Oscillator A Detune",
             0.0f, 100.0f, 0.1f, 1.0f, 12.0f, "cent");
    addFloat(layout, parameterIds::oscillatorASpread, "Oscillator A Spread",
             0.0f, 1.0f, 0.001f, 1.0f, 0.5f);
    addFloat(layout, parameterIds::oscillatorABlend, "Oscillator A Blend",
             0.0f, 1.0f, 0.001f, 1.0f, 0.5f);
    addBool(layout, parameterIds::oscillatorAPhaseReset, "Oscillator A Phase Reset", true);

    addFloat(layout, parameterIds::oscillatorBPosition, "Oscillator B Position",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addInteger(layout, parameterIds::oscillatorBCoarse, "Oscillator B Coarse", -36, 36, 0);
    addFloat(layout, parameterIds::oscillatorBFine, "Oscillator B Fine",
             -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "cent");
    addFloat(layout, parameterIds::oscillatorBPhase, "Oscillator B Phase",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addFloat(layout, parameterIds::oscillatorBRandomPhase, "Oscillator B Random Phase",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addFloat(layout, parameterIds::oscillatorBLevel, "Oscillator B Level",
             -60.0f, 0.0f, 0.01f, 1.0f, -60.0f, "dB");
    addFloat(layout, parameterIds::oscillatorBPan, "Oscillator B Pan",
             -1.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addInteger(layout, parameterIds::oscillatorBUnison, "Oscillator B Unison", 1, 8, 1);
    addFloat(layout, parameterIds::oscillatorBDetune, "Oscillator B Detune",
             0.0f, 100.0f, 0.1f, 1.0f, 12.0f, "cent");
    addFloat(layout, parameterIds::oscillatorBSpread, "Oscillator B Spread",
             0.0f, 1.0f, 0.001f, 1.0f, 0.5f);
    addFloat(layout, parameterIds::oscillatorBBlend, "Oscillator B Blend",
             0.0f, 1.0f, 0.001f, 1.0f, 0.5f);
    addBool(layout, parameterIds::oscillatorBPhaseReset, "Oscillator B Phase Reset", true);

    addChoice(layout, parameterIds::subWaveform, "Sub Waveform", {"Sine", "Triangle"}, 0);
    addInteger(layout, parameterIds::subOctave, "Sub Octave", -2, 0, -1);
    addChoice(layout, parameterIds::noiseType, "Noise Type", {"White", "Pink"}, 0);
    addFloat(layout, parameterIds::noiseLevel, "Noise Level",
             -60.0f, 0.0f, 0.01f, 1.0f, -60.0f, "dB");

    addChoice(layout, parameterIds::filterMode, "Filter Mode",
              {"Low-pass", "High-pass", "Band-pass"}, 0);
    addFloat(layout, parameterIds::filterResonance, "Filter Resonance",
             0.0f, 1.0f, 0.001f, 1.0f, 0.1f);
    addFloat(layout, parameterIds::filterDrive, "Filter Drive",
             0.0f, 24.0f, 0.01f, 1.0f, 0.0f, "dB");
    addFloat(layout, parameterIds::filterKeyTracking, "Filter Key Tracking",
             0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
    addFloat(layout, parameterIds::filterEnvelopeAmount, "Filter Envelope Amount",
             -8.0f, 8.0f, 0.01f, 1.0f, 0.0f, "oct");

    addEnvelope(layout, parameterIds::filterEnvelopeAttack, parameterIds::filterEnvelopeDecay,
                parameterIds::filterEnvelopeSustain, parameterIds::filterEnvelopeRelease,
                "Filter Env", synth::ParameterSnapshot{}.filterEnvelope);
    addEnvelope(layout, parameterIds::auxiliaryEnvelopeAttack, parameterIds::auxiliaryEnvelopeDecay,
                parameterIds::auxiliaryEnvelopeSustain, parameterIds::auxiliaryEnvelopeRelease,
                "Aux Env", synth::ParameterSnapshot{}.auxiliaryEnvelope);

    const juce::StringArray lfoShapes{"Sine", "Triangle", "Saw", "Square"};
    const juce::StringArray syncDivisions{"4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16"};
    for (std::size_t index = 0; index < parameterIds::lfoShape.size(); ++index)
    {
        const auto prefix = "LFO " + juce::String(static_cast<int>(index + 1));
        addChoice(layout, parameterIds::lfoShape[index], prefix + " Shape", lfoShapes, 0);
        addFloat(layout, parameterIds::lfoRate[index], prefix + " Rate",
                 0.01f, 30.0f, 0.01f, 0.3f, 1.0f, "Hz");
        addChoice(layout, parameterIds::lfoSyncDivision[index], prefix + " Division", syncDivisions, 4);
        addFloat(layout, parameterIds::lfoPhase[index], prefix + " Phase",
                 0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
        addBool(layout, parameterIds::lfoTempoSync[index], prefix + " Tempo Sync", false);
        addBool(layout, parameterIds::lfoRetrigger[index], prefix + " Retrigger", true);
    }
    return layout;
}

void PluginProcessor::prepareToPlay(double newSampleRate, int maximumExpectedSamplesPerBlock)
{
    activeSampleRate = newSampleRate;
    activeBlockSize = maximumExpectedSamplesPerBlock;
    engine.prepare(newSampleRate, maximumExpectedSamplesPerBlock);
    masterGain.reset(newSampleRate, 0.02);
    const auto initialGain = juce::Decibels::decibelsToGain(
        masterGainParameter->load(std::memory_order_relaxed), -60.0f);
    masterGain.setCurrentAndTargetValue(initialGain);
    directMidiPlayer.reset();
    previewMidiQueue.reset();
}

void PluginProcessor::releaseResources()
{
    activeSampleRate = 0.0;
    activeBlockSize = 0;
    engine.reset();
    directMidiPlayer.reset();
    previewMidiQueue.reset();
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

    const auto previewEvents = previewMidiQueue.renderBlock(midi, 0);
    juce::ignoreUnused(previewEvents);
    const auto directResult = directMidiPlayer.renderBlock(midi, audio.getNumSamples(), activeSampleRate);
    if (directResult.overflow)
        engine.panic();

    auto synthParameters = readSynthParameters();
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                synthParameters.tempoBpm = *bpm;
        }
    }
    engine.process(audio, midi, synthParameters);
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

juce::File PluginProcessor::writeAcceptedMidiToTemporaryFile() const
{
    const auto accepted = compositionSession.getAcceptedBundle();
    return accepted.has_value() ? midi::writeMidiToTemporaryFile(*accepted) : juce::File{};
}

juce::Result PluginProcessor::writeAcceptedMidiFile(const juce::File& destination) const
{
    const auto accepted = compositionSession.getAcceptedBundle();
    if (!accepted.has_value())
        return juce::Result::fail("Accept a composition before exporting MIDI");
    return midi::writeMidiFile(*accepted, destination);
}

juce::Result PluginProcessor::routeAcceptedMidi()
{
    const auto accepted = compositionSession.getAcceptedBundle();
    if (!accepted.has_value())
        return juce::Result::fail("Accept a composition before routing MIDI");
    return directMidiPlayer.publish(*accepted);
}

synth::ParameterSnapshot PluginProcessor::readSynthParameters() const noexcept
{
    const auto load = [](const std::atomic<float>* value)
    {
        return value->load(std::memory_order_relaxed);
    };
    const auto readOscillator = [&load](const OscillatorParameterPointers& source)
    {
        synth::OscillatorParameters result;
        result.position = load(source.position);
        result.coarseSemitones = load(source.coarse);
        result.fineCents = load(source.fine);
        result.phase = load(source.phase);
        result.randomPhase = load(source.randomPhase);
        result.levelDb = load(source.level);
        result.pan = load(source.pan);
        result.unisonVoices = juce::roundToInt(load(source.unison));
        result.unisonDetuneCents = load(source.detune);
        result.unisonSpread = load(source.spread);
        result.unisonBlend = load(source.blend);
        result.phaseReset = load(source.phaseReset) >= 0.5f;
        return result;
    };
    const auto readEnvelope = [&load](const EnvelopeParameterPointers& source)
    {
        return synth::EnvelopeParameters{load(source.attack), load(source.decay),
                                         load(source.sustain), load(source.release)};
    };

    synth::ParameterSnapshot snapshot;
    snapshot.oscillatorA = readOscillator(oscillatorAParameters);
    snapshot.oscillatorB = readOscillator(oscillatorBParameters);
    snapshot.legacyOscillatorAWaveform = juce::roundToInt(load(waveformParameter));
    snapshot.subWaveform = juce::roundToInt(load(subWaveformParameter));
    snapshot.subOctave = juce::roundToInt(load(subOctaveParameter));
    snapshot.subLevelDb = load(subLevelParameter);
    snapshot.noiseType = juce::roundToInt(load(noiseTypeParameter));
    snapshot.noiseLevelDb = load(noiseLevelParameter);
    snapshot.filterMode = static_cast<synth::FilterMode>(
        juce::jlimit(0, 2, juce::roundToInt(load(filterModeParameter))));
    snapshot.filterCutoffHz = load(cutoffParameter);
    snapshot.filterResonance = load(filterResonanceParameter);
    snapshot.filterDriveDb = load(filterDriveParameter);
    snapshot.filterKeyTracking = load(filterKeyTrackingParameter);
    snapshot.filterEnvelopeOctaves = load(filterEnvelopeAmountParameter);
    snapshot.ampEnvelope = readEnvelope(ampEnvelopeParameters);
    snapshot.filterEnvelope = readEnvelope(filterEnvelopeParameters);
    snapshot.auxiliaryEnvelope = readEnvelope(auxiliaryEnvelopeParameters);
    for (std::size_t index = 0; index < snapshot.lfos.size(); ++index)
    {
        snapshot.lfos[index].shape = static_cast<synth::LfoShape>(
            juce::jlimit(0, 3, juce::roundToInt(load(lfoParameters[index].shape))));
        snapshot.lfos[index].rateHz = load(lfoParameters[index].rate);
        snapshot.lfos[index].syncDivision = juce::roundToInt(load(lfoParameters[index].syncDivision));
        snapshot.lfos[index].phase = load(lfoParameters[index].phase);
        snapshot.lfos[index].tempoSync = load(lfoParameters[index].tempoSync) >= 0.5f;
        snapshot.lfos[index].retrigger = load(lfoParameters[index].retrigger) >= 0.5f;
    }
    return snapshot;
}

juce::Result PluginProcessor::setModulationRoutes(std::span<const synth::ModulationRoute> routes)
{
    if (const auto validation = synth::ModulationRegistry::validate(routes); validation.failed())
        return validation;
    const std::lock_guard lock(modulationStateMutex);
    if (modulationSnapshotsMatch(configuredModulationRoutes, routes))
        return juce::Result::ok();
    if (!engine.publishModulationRoutes(routes))
        return juce::Result::fail("A modulation update is already pending the next audio block");
    configuredModulationRoutes = synth::ModulationRegistry::makeSnapshot(routes);
    return juce::Result::ok();
}

synth::ModulationSnapshot PluginProcessor::getConfiguredModulationRoutes() const
{
    const std::lock_guard lock(modulationStateMutex);
    return configuredModulationRoutes;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto snapshot = parameters.copyState();
    if (const auto existingRoutes = snapshot.getChildWithName(modulationRoutesType); existingRoutes.isValid())
        snapshot.removeChild(existingRoutes, nullptr);
    snapshot.appendChild(serialiseModulationRoutes(getConfiguredModulationRoutes()), nullptr);
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

    synth::ModulationSnapshot candidateRoutes;
    const auto routeParsing = parseModulationRoutes(candidate, candidateRoutes);
    if (routeParsing.failed())
    {
        DBG("Folk Park state route parse failed: " + routeParsing.getErrorMessage());
        return;
    }
    const auto routePublication = setModulationRoutes(
        std::span{candidateRoutes.routes}.first(candidateRoutes.routeCount));
    if (routePublication.failed())
    {
        DBG("Folk Park state route publication failed: " + routePublication.getErrorMessage());
        return;
    }

    auto parameterCandidate = candidate.createCopy();
    if (const auto routes = parameterCandidate.getChildWithName(modulationRoutesType); routes.isValid())
        parameterCandidate.removeChild(routes, nullptr);
    parameters.replaceState(parameterCandidate);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new folkpark::PluginProcessor();
}
