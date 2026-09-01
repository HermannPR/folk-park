#include "PluginProcessor.h"
#include "ParameterIds.h"
#include "PluginEditor.h"
#include "persistence/CompositionJson.h"

#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <set>

namespace folkpark
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
const juce::Identifier modulationRoutesType{"ModulationRoutes"};
const juce::Identifier modulationRouteType{"ModulationRoute"};
const juce::Identifier projectSessionType{"FolkParkProjectSession"};
constexpr int oldestProjectSessionSchemaVersion = 1;
constexpr int projectSessionSchemaVersion = 2;
constexpr int maximumPluginStateBytes = 8 * 1024 * 1024;
constexpr auto projectSnapshotId = "00000000-0000-4000-8000-000000000001";
thread_local bool assistantParameterWriteOnThisThread = false;

template <typename Value>
void publishMaximum(std::atomic<Value>& destination, Value candidate) noexcept
{
    auto current = destination.load(std::memory_order_relaxed);
    while (candidate > current
           && !destination.compare_exchange_weak(current, candidate,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed))
    {
    }
}

std::uint64_t peakMicro(float peak) noexcept
{
    constexpr auto maximumTrackedPeak = 64.0f;
    const auto boundedPeak = std::isfinite(peak)
        ? juce::jlimit(0.0f, maximumTrackedPeak, peak) : maximumTrackedPeak;
    return static_cast<std::uint64_t>(std::llround(static_cast<double>(boundedPeak) * 1'000'000.0));
}

bool hasOnlyProjectSessionProperties(const juce::ValueTree& state)
{
    constexpr std::array allowed{
        "schemaVersion", "presetPayload", "currentPresetDirty",
        "acceptedCompositionPayload", "historyEntryId"};
    for (int index = 0; index < state.getNumProperties(); ++index)
    {
        const auto name = state.getPropertyName(index).toString();
        if (std::none_of(allowed.begin(), allowed.end(), [&name](const char* candidate)
                         { return name == candidate; }))
            return false;
    }
    return true;
}

bool hasOnlyProjectSessionChildren(const juce::ValueTree& state, int schemaVersion)
{
    if (schemaVersion == oldestProjectSessionSchemaVersion)
        return state.getNumChildren() == 0;
    if (state.getNumChildren() > 1)
        return false;
    return state.getNumChildren() == 0
        || state.getChild(0).hasType("FolkParkAssistantAudition");
}

std::vector<assistant::CurrentParameterValue> assistantValuesForPreset(
    const persistence::PresetDocument& document)
{
    std::vector<assistant::CurrentParameterValue> values;
    values.reserve(parameterIds::synthAndModulation.size() + parameterIds::allEffects.size());
    for (const auto& value : document.parameters)
        values.push_back({value.id, value.normalized});
    for (const auto& effect : document.effects)
        for (const auto& value : effect.parameters)
            values.push_back({value.id, value.normalized});
    return values;
}

juce::var binaryPayload(const juce::String& text)
{
    return juce::var(juce::MemoryBlock(text.toRawUTF8(),
                                      static_cast<std::size_t>(text.getNumBytesAsUTF8())));
}

bool readBinaryPayload(const juce::var& value,
                       std::int64_t maximumBytes,
                       juce::String& text)
{
    const auto* data = value.getBinaryData();
    if (!value.isBinaryData() || data == nullptr || data->isEmpty()
        || static_cast<std::int64_t>(data->getSize()) > maximumBytes
        || data->getSize() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    text = juce::String::fromUTF8(static_cast<const char*>(data->getData()),
                                 static_cast<int>(data->getSize()));
    return static_cast<std::size_t>(text.getNumBytesAsUTF8()) == data->getSize();
}

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

persistence::PersistenceConfiguration defaultPersistenceConfiguration()
{
    return {true,
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("Application Support")
                .getChildFile("folk park")};
}

int compositionNoteCount(const midi::CompositionBundle& bundle) noexcept
{
    std::size_t count = 0;
    for (const auto& clip : bundle.clips)
        count += clip.events.size();
    return static_cast<int>(juce::jmin<std::size_t>(
        count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

persistence::HistorySummary historySummary(const persistence::HistoryEntry& entry)
{
    return {entry.schemaVersion, entry.id, entry.parentId, entry.createdUnixMs,
            entry.updatedUnixMs, entry.generatorVersion, entry.storePromptSummary,
            entry.promptSummary, entry.presetId, entry.favorite, entry.tags, entry.deleted};
}
}

PluginProcessor::PluginProcessor()
    : PluginProcessor(defaultPersistenceConfiguration())
{
}

PluginProcessor::PluginProcessor(PersistenceConfiguration persistenceConfiguration)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "FolkParkState", createParameterLayout()),
      wavetableImportService([this](int oscillatorIndex,
                                    const synth::WavetableBank& bank,
                                    const synth::WavetableConverter::Metadata& metadata,
                                    const juce::File& source)
      {
          return publishImportedWavetable(oscillatorIndex, bank, metadata, source);
      })
{
    if (auto builtIn = synth::WavetableBank::createBuiltIn())
    {
        const auto preview = makeWavetableUiSnapshot(*builtIn);
        const auto immutable = std::shared_ptr<const synth::WavetableBank>(std::move(builtIn));
        wavetableUiSnapshots[0] = preview;
        wavetableUiSnapshots[1] = preview;
        currentWavetables[0] = immutable;
        currentWavetables[1] = immutable;
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
    effectParameters = {
        raw(parameterIds::distortionBypass), raw(parameterIds::distortionDrive),
        raw(parameterIds::distortionMix), raw(parameterIds::distortionOutput),
        raw(parameterIds::chorusBypass), raw(parameterIds::chorusRate),
        raw(parameterIds::chorusDepth), raw(parameterIds::chorusMix),
        raw(parameterIds::delayBypass), raw(parameterIds::delayDivision),
        raw(parameterIds::delayFeedback), raw(parameterIds::delayMix),
        raw(parameterIds::reverbBypass), raw(parameterIds::reverbRoomSize),
        raw(parameterIds::reverbDamping), raw(parameterIds::reverbMix),
        raw(parameterIds::compressorBypass), raw(parameterIds::compressorThreshold),
        raw(parameterIds::compressorRatio), raw(parameterIds::compressorAttack),
        raw(parameterIds::compressorRelease), raw(parameterIds::compressorMakeup),
        raw(parameterIds::compressorMix), raw(parameterIds::eqBypass),
        raw(parameterIds::eqLowGain), raw(parameterIds::eqMidFrequency),
        raw(parameterIds::eqMidGain), raw(parameterIds::eqMidQ), raw(parameterIds::eqHighGain)};

    persistence::PresetMetadata defaultsMetadata;
    defaultsMetadata.id = "00000000-0000-4000-8000-000000000000";
    defaultsMetadata.name = "Migration defaults";
    auto migrationDefaults = captureCurrentPreset(defaultsMetadata);
    persistenceCoordinator = std::make_unique<persistence::PersistenceCoordinator>(
        std::move(persistenceConfiguration), std::move(migrationDefaults));
    for (const auto* id : parameterIds::synthAndModulation)
        parameters.addParameterListener(id, this);
    for (const auto* id : parameterIds::allEffects)
        parameters.addParameterListener(id, this);
    cleanParameterRevision.store(parameterRevision.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto* id : parameterIds::synthAndModulation)
        parameters.removeParameterListener(id, this);
    for (const auto* id : parameterIds::allEffects)
        parameters.removeParameterListener(id, this);
}

bool PluginProcessor::publishWavetable(int oscillatorIndex,
                                       const synth::WavetableBank& bank)
{
    if (oscillatorIndex < 0 || oscillatorIndex >= static_cast<int>(wavetableUiSnapshots.size())
        || !bank.isFiniteAndNormalised())
        return false;
    const auto immutable = std::make_shared<const synth::WavetableBank>(bank);
    if (!engine.publishWavetable(oscillatorIndex, bank))
        return false;
    const auto preview = makeWavetableUiSnapshot(bank);
    {
        const std::lock_guard lock(wavetableUiMutex);
        const auto index = static_cast<std::size_t>(oscillatorIndex);
        wavetableUiSnapshots[index] = preview;
        currentWavetables[index] = immutable;
        currentWavetableAssets[index].reset();
    }
    if (persistenceCoordinator != nullptr)
        persistenceCoordinator->markCurrentSoundDirty(
            "Wavetable changed; save the sound to retain it as a preset");
    return true;
}

juce::Result PluginProcessor::publishImportedWavetable(
    int oscillatorIndex,
    const synth::WavetableBank& bank,
    const synth::WavetableConverter::Metadata& metadata,
    const juce::File& source)
{
    if (oscillatorIndex < 0 || oscillatorIndex > 1 || !source.existsAsFile()
        || metadata.sourceSha256.isEmpty())
        return juce::Result::fail("Confirmed wavetable source metadata is incomplete");

    std::optional<persistence::AssetReference> retained;
    if (persistenceCoordinator != nullptr && persistenceCoordinator->status().enabled)
    {
        persistence::AssetReference reference;
        const auto slot = oscillatorIndex == 0 ? persistence::AssetSlot::oscillatorA
                                               : persistence::AssetSlot::oscillatorB;
        const auto imported = persistenceCoordinator->importWavetableSource(source, slot, reference);
        if (imported.failed())
            return imported;
        if (reference.sha256 != metadata.sourceSha256.toLowerCase())
            return juce::Result::fail("Confirmed WAV changed between preview and retention");
        retained = reference;
    }
    if (!publishWavetable(oscillatorIndex, bank))
        return juce::Result::fail("Audio exchange is busy; confirm again after the pending crossfade");
    if (retained.has_value())
    {
        const std::lock_guard lock(wavetableUiMutex);
        currentWavetableAssets[static_cast<std::size_t>(oscillatorIndex)] = std::move(retained);
    }
    if (persistenceCoordinator != nullptr)
        persistenceCoordinator->markCurrentSoundDirty(
            "Confirmed WAV retained; save the current sound to create a restorable preset");
    return juce::Result::ok();
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

    // M5 effects are append-only and bypassed by default so older states stay gain-safe.
    addBool(layout, parameterIds::distortionBypass, "Distortion Bypass", true);
    addFloat(layout, parameterIds::distortionDrive, "Distortion Drive", 0.0f, 36.0f, 0.01f, 1.0f, 6.0f, "dB");
    addFloat(layout, parameterIds::distortionMix, "Distortion Mix", 0.0f, 1.0f, 0.001f, 1.0f, 0.5f);
    addFloat(layout, parameterIds::distortionOutput, "Distortion Output", -24.0f, 0.0f, 0.01f, 1.0f, -6.0f, "dB");
    addBool(layout, parameterIds::chorusBypass, "Chorus Bypass", true);
    addFloat(layout, parameterIds::chorusRate, "Chorus Rate", 0.05f, 5.0f, 0.01f, 0.4f, 0.35f, "Hz");
    addFloat(layout, parameterIds::chorusDepth, "Chorus Depth", 0.0f, 20.0f, 0.01f, 1.0f, 6.0f, "ms");
    addFloat(layout, parameterIds::chorusMix, "Chorus Mix", 0.0f, 1.0f, 0.001f, 1.0f, 0.25f);
    addBool(layout, parameterIds::delayBypass, "Delay Bypass", true);
    addChoice(layout, parameterIds::delayDivision, "Delay Division", {"1 bar", "1/2", "1/4", "1/8", "1/16"}, 2);
    addFloat(layout, parameterIds::delayFeedback, "Delay Feedback", 0.0f, 0.85f, 0.001f, 1.0f, 0.3f);
    addFloat(layout, parameterIds::delayMix, "Delay Mix", 0.0f, 1.0f, 0.001f, 1.0f, 0.25f);
    addBool(layout, parameterIds::reverbBypass, "Reverb Bypass", true);
    addFloat(layout, parameterIds::reverbRoomSize, "Reverb Room Size", 0.0f, 1.0f, 0.001f, 1.0f, 0.45f);
    addFloat(layout, parameterIds::reverbDamping, "Reverb Damping", 0.0f, 1.0f, 0.001f, 1.0f, 0.4f);
    addFloat(layout, parameterIds::reverbMix, "Reverb Mix", 0.0f, 1.0f, 0.001f, 1.0f, 0.2f);
    addBool(layout, parameterIds::compressorBypass, "Compressor Bypass", true);
    addFloat(layout, parameterIds::compressorThreshold, "Compressor Threshold", -60.0f, 0.0f, 0.01f, 1.0f, -18.0f, "dB");
    addFloat(layout, parameterIds::compressorRatio, "Compressor Ratio", 1.0f, 20.0f, 0.01f, 0.5f, 4.0f);
    addFloat(layout, parameterIds::compressorAttack, "Compressor Attack", 0.1f, 100.0f, 0.1f, 0.4f, 10.0f, "ms");
    addFloat(layout, parameterIds::compressorRelease, "Compressor Release", 10.0f, 1000.0f, 1.0f, 0.4f, 100.0f, "ms");
    addFloat(layout, parameterIds::compressorMakeup, "Compressor Makeup", 0.0f, 18.0f, 0.01f, 1.0f, 0.0f, "dB");
    addFloat(layout, parameterIds::compressorMix, "Compressor Mix", 0.0f, 1.0f, 0.001f, 1.0f, 1.0f);
    addBool(layout, parameterIds::eqBypass, "EQ Bypass", true);
    addFloat(layout, parameterIds::eqLowGain, "EQ Low Shelf", -18.0f, 18.0f, 0.01f, 1.0f, 0.0f, "dB");
    addFloat(layout, parameterIds::eqMidFrequency, "EQ Mid Frequency", 20.0f, 20000.0f, 1.0f, 0.25f, 1000.0f, "Hz");
    addFloat(layout, parameterIds::eqMidGain, "EQ Mid Gain", -18.0f, 18.0f, 0.01f, 1.0f, 0.0f, "dB");
    addFloat(layout, parameterIds::eqMidQ, "EQ Mid Q", 0.1f, 10.0f, 0.01f, 0.4f, 1.0f);
    addFloat(layout, parameterIds::eqHighGain, "EQ High Shelf", -18.0f, 18.0f, 0.01f, 1.0f, 0.0f, "dB");
    return layout;
}

void PluginProcessor::prepareToPlay(double newSampleRate, int maximumExpectedSamplesPerBlock)
{
    activeSampleRate.store(newSampleRate, std::memory_order_release);
    activeBlockSize.store(maximumExpectedSamplesPerBlock, std::memory_order_release);
    engine.prepare(newSampleRate, maximumExpectedSamplesPerBlock);
    effectChain.prepare(newSampleRate, maximumExpectedSamplesPerBlock);
    masterGain.reset(newSampleRate, 0.02);
    const auto initialGain = juce::Decibels::decibelsToGain(
        masterGainParameter->load(std::memory_order_relaxed), -60.0f);
    masterGain.setCurrentAndTargetValue(initialGain);
    directMidiPlayer.reset();
    previewMidiQueue.reset();
}

void PluginProcessor::releaseResources()
{
    activeSampleRate.store(0.0, std::memory_order_release);
    activeBlockSize.store(0, std::memory_order_release);
    engine.reset();
    effectChain.reset();
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
    if (panicRequested.exchange(false, std::memory_order_acq_rel))
        engine.panic();

    const auto previewEvents = previewMidiQueue.renderBlock(midi, 0);
    juce::ignoreUnused(previewEvents);
    const auto directResult = directMidiPlayer.renderBlock(
        midi, audio.getNumSamples(), activeSampleRate.load(std::memory_order_relaxed));
    if (directResult.overflow)
    {
        directMidiOverflows.fetch_add(1, std::memory_order_relaxed);
        engine.panic();
    }

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
    effectChain.process(audio, readEffectsParameters(synthParameters.tempoBpm));
    masterGain.setTargetValue(juce::Decibels::decibelsToGain(
        masterGainParameter->load(std::memory_order_relaxed), -60.0f));

    const auto channelCount = juce::jmin(2, audio.getNumChannels());
    std::uint64_t containedSamples = 0;
    std::uint64_t overUnitySamples = 0;
    auto preMasterPeak = 0.0f;
    auto outputPeak = 0.0f;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto gain = masterGain.getNextValue();
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto preMaster = audio.getSample(channel, sample);
            preMasterPeak = std::max(preMasterPeak, std::abs(preMaster));
            const auto output = preMaster * gain;
            if (std::isfinite(output))
            {
                audio.setSample(channel, sample, output);
                const auto magnitude = std::abs(output);
                outputPeak = std::max(outputPeak, magnitude);
                overUnitySamples += magnitude > 1.0f ? 1u : 0u;
            }
            else
            {
                audio.setSample(channel, sample, 0.0f);
                ++containedSamples;
            }
        }
    }
    publishMaximum(maximumPreMasterPeakMicro, peakMicro(preMasterPeak));
    publishMaximum(maximumOutputPeakMicro, peakMicro(outputPeak));
    publishMaximum(maximumActiveVoices, engine.getActiveVoiceCount());
    if (overUnitySamples != 0)
        overUnityOutputSamples.fetch_add(overUnitySamples, std::memory_order_relaxed);
    if (containedSamples != 0)
        nonFiniteOutputSamples.fetch_add(containedSamples, std::memory_order_relaxed);
}

bool PluginProcessor::previewNoteOn(int note, int velocity) noexcept
{
    if (previewMidiQueue.enqueueNoteOn(note, velocity))
        return true;
    previewMidiOverflows.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool PluginProcessor::previewNoteOff(int note) noexcept
{
    if (previewMidiQueue.enqueueNoteOff(note))
        return true;
    previewMidiOverflows.fetch_add(1, std::memory_order_relaxed);
    return false;
}

diagnostics::Snapshot PluginProcessor::getDiagnosticsSnapshot() const
{
    diagnostics::Snapshot snapshot;
    snapshot.productVersion = FOLK_PARK_VERSION;
    snapshot.buildType = FOLK_PARK_BUILD_TYPE;
    snapshot.architecture = "x86_64";
    snapshot.wrapperFormat = getWrapperTypeDescription(wrapperType);
    snapshot.hostName = juce::PluginHostType().getHostDescription();
    snapshot.hostVersion = "unavailable";
    snapshot.sampleRate = activeSampleRate.load(std::memory_order_acquire);
    snapshot.maximumBlockSize = activeBlockSize.load(std::memory_order_acquire);
    snapshot.activeVoices = getActiveVoiceCount();
    snapshot.maximumActiveVoices = maximumActiveVoices.load(std::memory_order_relaxed);

    const auto persistence = getPersistenceStatus();
    if (!persistence.enabled)
    {
        snapshot.preset = diagnostics::ServiceCode::unavailable;
        snapshot.database = diagnostics::ServiceCode::unavailable;
    }
    else
    {
        snapshot.preset = !persistence.missingAssets.empty()
            ? diagnostics::ServiceCode::missingAssets
            : (persistence.presetAvailable ? diagnostics::ServiceCode::ready
                                           : diagnostics::ServiceCode::degraded);
        snapshot.database = persistence.historyAvailable ? diagnostics::ServiceCode::ready
                                                         : diagnostics::ServiceCode::degraded;
    }
    snapshot.provider = diagnostics::ServiceCode::disabled;
    snapshot.uiBridge = diagnostics::ServiceCode::ready;
    snapshot.nonFiniteOutputSamples = nonFiniteOutputSamples.load(std::memory_order_relaxed);
    snapshot.overUnityOutputSamples = overUnityOutputSamples.load(std::memory_order_relaxed);
    snapshot.maximumPreMasterPeakMicro = maximumPreMasterPeakMicro.load(std::memory_order_relaxed);
    snapshot.maximumOutputPeakMicro = maximumOutputPeakMicro.load(std::memory_order_relaxed);
    snapshot.voiceSteals = engine.getVoiceStealCount();
    snapshot.directMidiOverflows = directMidiOverflows.load(std::memory_order_relaxed);
    snapshot.previewMidiOverflows = previewMidiOverflows.load(std::memory_order_relaxed);
    snapshot.rejectedProjectStates = rejectedProjectStates.load(std::memory_order_relaxed);
    return snapshot;
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

juce::Result PluginProcessor::requestAcceptedWavRender(const juce::File& destination,
                                                       bool allowOverwrite)
{
    const auto accepted = compositionSession.getAcceptedBundle();
    if (!accepted.has_value())
        return juce::Result::fail("Accept a composition before rendering WAV audio");
    return offlinePreviewService.request(*accepted,
        makeOfflinePreviewSnapshot(accepted->intent.tempoBpm), destination, allowOverwrite);
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

effects::Parameters PluginProcessor::readEffectsParameters(double tempoBpm) const noexcept
{
    const auto load = [](const std::atomic<float>* value) { return value->load(std::memory_order_relaxed); };
    effects::Parameters result;
    result.distortionBypass = load(effectParameters.distortionBypass) >= 0.5f;
    result.distortionDriveDb = load(effectParameters.distortionDrive);
    result.distortionMix = load(effectParameters.distortionMix);
    result.distortionOutputDb = load(effectParameters.distortionOutput);
    result.chorusBypass = load(effectParameters.chorusBypass) >= 0.5f;
    result.chorusRateHz = load(effectParameters.chorusRate);
    result.chorusDepthMs = load(effectParameters.chorusDepth);
    result.chorusMix = load(effectParameters.chorusMix);
    result.delayBypass = load(effectParameters.delayBypass) >= 0.5f;
    result.delayDivision = juce::roundToInt(load(effectParameters.delayDivision));
    result.delayFeedback = load(effectParameters.delayFeedback);
    result.delayMix = load(effectParameters.delayMix);
    result.reverbBypass = load(effectParameters.reverbBypass) >= 0.5f;
    result.reverbRoomSize = load(effectParameters.reverbRoomSize);
    result.reverbDamping = load(effectParameters.reverbDamping);
    result.reverbMix = load(effectParameters.reverbMix);
    result.compressorBypass = load(effectParameters.compressorBypass) >= 0.5f;
    result.compressorThresholdDb = load(effectParameters.compressorThreshold);
    result.compressorRatio = load(effectParameters.compressorRatio);
    result.compressorAttackMs = load(effectParameters.compressorAttack);
    result.compressorReleaseMs = load(effectParameters.compressorRelease);
    result.compressorMakeupDb = load(effectParameters.compressorMakeup);
    result.compressorMix = load(effectParameters.compressorMix);
    result.eqBypass = load(effectParameters.eqBypass) >= 0.5f;
    result.eqLowGainDb = load(effectParameters.eqLowGain);
    result.eqMidFrequencyHz = load(effectParameters.eqMidFrequency);
    result.eqMidGainDb = load(effectParameters.eqMidGain);
    result.eqMidQ = load(effectParameters.eqMidQ);
    result.eqHighGainDb = load(effectParameters.eqHighGain);
    result.tempoBpm = tempoBpm;
    return result;
}

render::OfflinePreviewSnapshot PluginProcessor::makeOfflinePreviewSnapshot(double tempoBpm) const
{
    render::OfflinePreviewSnapshot snapshot;
    snapshot.synthParameters = readSynthParameters();
    snapshot.synthParameters.tempoBpm = tempoBpm;
    snapshot.effectParameters = readEffectsParameters(tempoBpm);
    snapshot.modulation = getConfiguredModulationRoutes();
    snapshot.masterGainDb = masterGainParameter->load(std::memory_order_relaxed);
    const std::lock_guard lock(wavetableUiMutex);
    snapshot.wavetableA = currentWavetables[0];
    snapshot.wavetableB = currentWavetables[1];
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
    if (persistenceCoordinator != nullptr)
        persistenceCoordinator->markCurrentSoundDirty(
            "Modulation routes changed; save the current sound to retain them");
    return juce::Result::ok();
}

synth::ModulationSnapshot PluginProcessor::getConfiguredModulationRoutes() const
{
    const std::lock_guard lock(modulationStateMutex);
    return configuredModulationRoutes;
}

std::vector<assistant::CurrentParameterValue>
PluginProcessor::getAssistantParameterSnapshot() const
{
    std::vector<assistant::CurrentParameterValue> snapshot;
    snapshot.reserve(parameterIds::synthAndModulation.size() + parameterIds::allEffects.size());
    const auto capture = [this, &snapshot](const auto& ids)
    {
        for (const auto* id : ids)
            if (const auto* parameter = parameters.getParameter(id))
                snapshot.push_back({id, parameter->getValue()});
    };
    capture(parameterIds::synthAndModulation);
    capture(parameterIds::allEffects);
    return snapshot;
}

void PluginProcessor::applyAssistantParameterValues(
    std::span<const assistant::CurrentParameterValue> values)
{
    undoManager.beginNewTransaction("Audition Jarvis sound proposal");
    assistantParameterWriteOnThisThread = true;
    for (const auto& value : values)
        if (auto* parameter = parameters.getParameter(value.parameterId))
            parameter->setValueNotifyingHost(value.normalized);
    assistantParameterWriteOnThisThread = false;
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    if (!assistantParameterWriteOnThisThread)
        parameterRevision.fetch_add(1, std::memory_order_relaxed);
}

juce::Result PluginProcessor::beginAssistantProposal(
    const assistant::ParameterProposal& proposal)
{
    const std::lock_guard lock(assistantAuditionMutex);
    if (const auto validation = assistant::validateParameterProposal(proposal);
        validation.failed())
        return validation;
    const auto revisionBefore = parameterRevision.load(std::memory_order_acquire);
    const auto current = getAssistantParameterSnapshot();
    if (revisionBefore != parameterRevision.load(std::memory_order_acquire))
        return juce::Result::fail("The sound changed while the assistant proposal was opening");
    auto hostCanonicalProposal = proposal;
    constexpr float valueTolerance = 1.0e-6f;
    for (const auto& change : hostCanonicalProposal.changes)
    {
        const auto currentValue = std::find_if(current.begin(), current.end(),
            [&change](const auto& value) { return value.parameterId == change.parameterId; });
        if (currentValue == current.end()
            || std::abs(currentValue->normalized - change.currentNormalized) > valueTolerance)
            return juce::Result::fail("Assistant proposal is stale for the current sound");
    }
    std::erase_if(hostCanonicalProposal.changes, [this, &current](auto& change)
    {
        auto* parameter = parameters.getParameter(change.parameterId);
        if (parameter == nullptr)
            return false;
        const auto& range = parameter->getNormalisableRange();
        const auto hostValue = range.snapToLegalValue(
            parameter->convertFrom0to1(change.proposedNormalized));
        change.proposedNormalized = parameter->convertTo0to1(hostValue);
        const auto currentValue = std::find_if(current.begin(), current.end(),
            [&change](const auto& value) { return value.parameterId == change.parameterId; });
        return currentValue != current.end()
            && std::abs(currentValue->normalized - change.proposedNormalized) <= valueTolerance;
    });
    if (hostCanonicalProposal.changes.empty())
        return juce::Result::fail("The current sound already matches this Jarvis proposal");
    const auto begun = assistantAudition.begin(hostCanonicalProposal, current);
    if (begun.wasOk())
        assistantRevisionBoundary = AssistantRevisionBoundary{revisionBefore, revisionBefore};
    return begun;
}

juce::Result PluginProcessor::auditionAssistantSide(assistant::AuditionSide side)
{
    const std::lock_guard lock(assistantAuditionMutex);
    if (!assistantRevisionBoundary.has_value())
        return juce::Result::fail("No assistant A/B proposal is active");
    if (parameterRevision.load(std::memory_order_acquire)
        != assistantRevisionBoundary->expected)
    {
        assistantAudition.invalidate(
            "Assistant A/B stopped because the host sound changed outside the session");
        assistantRevisionBoundary.reset();
        return juce::Result::fail(assistantAudition.snapshot().message);
    }
    const auto current = getAssistantParameterSnapshot();
    const auto result = assistantAudition.audition(side, current, [this](auto values)
    {
        applyAssistantParameterValues(values);
    });
    if (result.wasOk())
        assistantRevisionBoundary->expected = parameterRevision.load(std::memory_order_acquire);
    else if (!assistantAudition.snapshot().active())
        assistantRevisionBoundary.reset();
    return result;
}

juce::Result PluginProcessor::acceptAssistantProposal()
{
    const std::lock_guard lock(assistantAuditionMutex);
    if (!assistantRevisionBoundary.has_value())
        return juce::Result::fail("No assistant A/B proposal is active");
    if (parameterRevision.load(std::memory_order_acquire)
        != assistantRevisionBoundary->expected)
    {
        assistantAudition.invalidate(
            "Assistant A/B stopped because the host sound changed outside the session");
        assistantRevisionBoundary.reset();
        return juce::Result::fail(assistantAudition.snapshot().message);
    }
    const auto current = getAssistantParameterSnapshot();
    const auto result = assistantAudition.accept(current, [this](auto values)
    {
        applyAssistantParameterValues(values);
    });
    if (result.wasOk())
        parameterRevision.fetch_add(1, std::memory_order_release);
    if (result.wasOk() || !assistantAudition.snapshot().active())
        assistantRevisionBoundary.reset();
    return result;
}

juce::Result PluginProcessor::rejectAssistantProposal()
{
    const std::lock_guard lock(assistantAuditionMutex);
    if (!assistantRevisionBoundary.has_value())
        return juce::Result::fail("No assistant A/B proposal is active");
    if (parameterRevision.load(std::memory_order_acquire)
        != assistantRevisionBoundary->expected)
    {
        assistantAudition.invalidate(
            "Assistant A/B stopped because the host sound changed outside the session");
        assistantRevisionBoundary.reset();
        return juce::Result::fail(assistantAudition.snapshot().message);
    }
    const auto current = getAssistantParameterSnapshot();
    const auto result = assistantAudition.reject(current, [this](auto values)
    {
        applyAssistantParameterValues(values);
    });
    if (result.wasOk() || !assistantAudition.snapshot().active())
        assistantRevisionBoundary.reset();
    return result;
}

assistant::AssistantAuditionSnapshot PluginProcessor::getAssistantAuditionSnapshot() const
{
    const std::lock_guard lock(assistantAuditionMutex);
    return assistantAudition.snapshot();
}

void PluginProcessor::resetAssistantAudition()
{
    const std::lock_guard lock(assistantAuditionMutex);
    assistantAudition.reset();
    assistantRevisionBoundary.reset();
}

persistence::PresetDocument PluginProcessor::captureCurrentPreset(
    const persistence::PresetMetadata& metadata) const
{
    auto document = persistence::makePresetTemplate(
        FOLK_PARK_VERSION, metadata.id, metadata.name);
    document.metadata = metadata;
    for (auto& value : document.parameters)
        if (const auto* parameter = parameters.getParameter(value.id))
            value.normalized = parameter->getValue();
    for (auto& effect : document.effects)
        for (auto& value : effect.parameters)
            if (const auto* parameter = parameters.getParameter(value.id))
                value.normalized = parameter->getValue();
    const auto routes = getConfiguredModulationRoutes();
    document.modulationRoutes.assign(
        routes.routes.begin(), routes.routes.begin() + static_cast<std::ptrdiff_t>(routes.routeCount));
    {
        const std::lock_guard lock(wavetableUiMutex);
        for (const auto& reference : currentWavetableAssets)
            if (reference.has_value())
                document.assets.push_back(*reference);
    }
    return document;
}

juce::Result PluginProcessor::initialisePersistence()
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    return persistenceCoordinator->initialise();
}

persistence::PersistenceStatusSnapshot PluginProcessor::getPersistenceStatus() const
{
    auto snapshot = persistenceCoordinator != nullptr
        ? persistenceCoordinator->status()
        : persistence::PersistenceStatusSnapshot{};
    if (parameterRevision.load(std::memory_order_acquire)
        != cleanParameterRevision.load(std::memory_order_acquire))
        snapshot.currentPresetDirty = true;
    return snapshot;
}

persistence::PresetLibraryResult PluginProcessor::listPresets()
{
    if (persistenceCoordinator == nullptr)
        return {juce::Result::fail("Persistence coordinator is unavailable"), {}};
    return persistenceCoordinator->listPresets();
}

juce::Result PluginProcessor::saveCurrentPreset(const PresetSaveRequest& request)
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    const std::lock_guard assistantLock(assistantAuditionMutex);
    if (assistantAudition.snapshot().active())
        return juce::Result::fail(
            "Accept or reject the active assistant A/B proposal before saving a preset");
    persistence::PresetMetadata metadata;
    const auto currentId = persistenceCoordinator->currentPresetId();
    metadata.id = request.allowOverwrite && midi::isUuid(currentId)
        ? currentId
        : juce::Uuid().toDashedString();
    metadata.name = request.name;
    metadata.author = request.author;
    metadata.tags = request.tags;
    metadata.genre = request.genre;
    metadata.emotion = request.emotion;
    metadata.description = request.description;
    metadata.favorite = request.favorite;
    const auto capturedRevision = parameterRevision.load(std::memory_order_acquire);
    const auto document = captureCurrentPreset(metadata);
    if (const auto validation = persistence::validatePreset(document); validation.failed())
        return validation;
    const auto saved = persistenceCoordinator->savePreset(document, request.allowOverwrite);
    if (saved.wasOk())
        cleanParameterRevision.store(capturedRevision, std::memory_order_release);
    return saved;
}

juce::Result PluginProcessor::applyPresetCandidate(
    const persistence::PresetCandidateResult& candidate)
{
    if (!candidate.readyToApply())
        return candidate.status.failed()
            ? candidate.status
            : juce::Result::fail("Preset has missing assets and cannot be applied yet");
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");

    auto prepared = persistenceCoordinator->prepareWavetables(candidate.document);
    if (!prepared.succeeded())
        return prepared.status;
    const auto routes = std::span{candidate.document.modulationRoutes};
    if (!engine.publishPresetSnapshot(*prepared.banks[0], *prepared.banks[1], routes))
        return juce::Result::fail(
            "Live sound update is busy; retry after the next audio block");

    undoManager.beginNewTransaction("Load folk park preset");
    for (const auto& value : candidate.document.parameters)
        if (auto* parameter = parameters.getParameter(value.id))
            parameter->setValueNotifyingHost(value.normalized);
    for (const auto& effect : candidate.document.effects)
        for (const auto& value : effect.parameters)
            if (auto* parameter = parameters.getParameter(value.id))
                parameter->setValueNotifyingHost(value.normalized);

    {
        const std::lock_guard lock(modulationStateMutex);
        configuredModulationRoutes = synth::ModulationRegistry::makeSnapshot(routes);
    }
    const auto previewA = makeWavetableUiSnapshot(*prepared.banks[0]);
    const auto previewB = makeWavetableUiSnapshot(*prepared.banks[1]);
    auto immutableA = std::shared_ptr<const synth::WavetableBank>(std::move(prepared.banks[0]));
    auto immutableB = std::shared_ptr<const synth::WavetableBank>(std::move(prepared.banks[1]));
    {
        const std::lock_guard lock(wavetableUiMutex);
        wavetableUiSnapshots[0] = previewA;
        wavetableUiSnapshots[1] = previewB;
        currentWavetables[0] = std::move(immutableA);
        currentWavetables[1] = std::move(immutableB);
        currentWavetableAssets[0].reset();
        currentWavetableAssets[1].reset();
        for (const auto& reference : candidate.document.assets)
        {
            const auto index = reference.slot == persistence::AssetSlot::oscillatorA ? 0U : 1U;
            currentWavetableAssets[index] = reference;
        }
    }
    requestPanic();
    persistenceCoordinator->markPresetApplied(candidate.document);
    cleanParameterRevision.store(parameterRevision.load(std::memory_order_acquire),
                                 std::memory_order_release);
    resetAssistantAudition();
    return juce::Result::ok();
}

void PluginProcessor::clearPendingProjectRestore()
{
    const std::lock_guard lock(projectStateMutex);
    pendingProjectRestore.reset();
}

juce::Result PluginProcessor::completeProjectRestore(
    const persistence::PresetDocument& document,
    std::optional<midi::CompositionBundle> acceptedBundle,
    std::optional<assistant::AssistantAuditionSnapshot> assistantSnapshot,
    bool dirty,
    const juce::String& historyEntryId)
{
    std::optional<assistant::AssistantAuditionSession> preparedAssistant;
    if (assistantSnapshot.has_value())
    {
        preparedAssistant.emplace();
        const auto presetValues = assistantValuesForPreset(document);
        if (const auto restored = preparedAssistant->restore(*assistantSnapshot, presetValues);
            restored.failed())
            return restored;
    }
    if (const auto restored = compositionSession.restoreProjectState(acceptedBundle);
        restored.failed())
        return restored;
    {
        const std::lock_guard lock(historyLineageMutex);
        lastHistoryEntryId = historyEntryId;
        lastHistoryClipIds.clear();
        if (acceptedBundle.has_value())
        {
            lastHistoryClipIds.reserve(acceptedBundle->clips.size());
            for (const auto& clip : acceptedBundle->clips)
                lastHistoryClipIds.push_back(clip.id);
        }
    }
    cleanParameterRevision.store(parameterRevision.load(std::memory_order_acquire),
                                 std::memory_order_release);
    if (persistenceCoordinator != nullptr)
        persistenceCoordinator->restoreSessionStatus(document, dirty);
    if (preparedAssistant.has_value())
    {
        const std::lock_guard lock(assistantAuditionMutex);
        assistantAudition = std::move(*preparedAssistant);
        const auto revision = parameterRevision.load(std::memory_order_acquire);
        assistantRevisionBoundary = AssistantRevisionBoundary{revision, revision};
    }
    else
    {
        resetAssistantAudition();
    }
    clearPendingProjectRestore();
    return juce::Result::ok();
}

juce::Result PluginProcessor::loadLibraryPreset(const juce::String& presetId)
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    clearPendingProjectRestore();
    const auto candidate = persistenceCoordinator->loadLibraryPreset(presetId);
    return applyPresetCandidate(candidate);
}

juce::Result PluginProcessor::importExternalPreset(const juce::File& file)
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    clearPendingProjectRestore();
    const auto candidate = persistenceCoordinator->importExternalPreset(file);
    return applyPresetCandidate(candidate);
}

juce::Result PluginProcessor::relinkPendingPresetAsset(persistence::AssetSlot slot,
                                                       const juce::File& selectedFile)
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    const auto candidate = persistenceCoordinator->relinkPendingAsset(slot, selectedFile);
    const auto applied = applyPresetCandidate(candidate);
    if (applied.failed())
        return applied;
    std::optional<PendingProjectRestore> pending;
    {
        const std::lock_guard lock(projectStateMutex);
        pending = pendingProjectRestore;
    }
    return pending.has_value()
        ? completeProjectRestore(candidate.document, std::move(pending->acceptedBundle),
                                 std::move(pending->assistantSnapshot), pending->dirty,
                                 pending->historyEntryId)
        : juce::Result::ok();
}

juce::Result PluginProcessor::setPresetFavorite(const juce::String& presetId, bool favorite)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->setPresetFavorite(presetId, favorite)
        : juce::Result::fail("Persistence coordinator is unavailable");
}

juce::Result PluginProcessor::acceptCompositionCandidate()
{
    const auto accepted = compositionSession.acceptCandidate();
    if (accepted.failed())
        return accepted;
    if (const auto bundle = compositionSession.getAcceptedBundle(); bundle.has_value())
        recordAcceptedComposition(*bundle);
    return juce::Result::ok();
}

void PluginProcessor::recordAcceptedComposition(const midi::CompositionBundle& bundle)
{
    if (persistenceCoordinator == nullptr || !persistenceCoordinator->status().enabled)
        return;
    persistence::HistoryEntry entry;
    entry.id = juce::Uuid().toDashedString();
    entry.createdUnixMs = juce::Time::currentTimeMillis();
    entry.updatedUnixMs = entry.createdUnixMs;
    entry.generatorVersion = midi::compositionGeneratorVersion;
    entry.storePromptSummary = false;
    entry.macroSnapshot = bundle.intent;
    entry.composition = bundle;
    entry.presetId = persistenceCoordinator->currentPresetId();
    entry.tags = {midi::stableId(bundle.intent.genreProfile),
                  midi::stableId(bundle.intent.emotion)};
    {
        const std::lock_guard lock(historyLineageMutex);
        const std::set<juce::String> parents(lastHistoryClipIds.begin(), lastHistoryClipIds.end());
        const auto isVariation = std::any_of(bundle.clips.begin(), bundle.clips.end(),
            [&parents](const auto& clip)
            {
                return !clip.parentClipId.isEmpty() && parents.contains(clip.parentClipId);
            });
        if (isVariation)
            entry.parentId = lastHistoryEntryId;
    }
    if (persistenceCoordinator->storeHistory(entry).failed())
        return;
    const std::lock_guard lock(historyLineageMutex);
    lastHistoryEntryId = entry.id;
    lastHistoryClipIds.clear();
    lastHistoryClipIds.reserve(bundle.clips.size());
    for (const auto& clip : bundle.clips)
        lastHistoryClipIds.push_back(clip.id);
}

persistence::HistorySearchResult PluginProcessor::searchHistory(
    const persistence::HistorySearchQuery& query)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->searchHistory(query)
        : persistence::HistorySearchResult{juce::Result::fail(
              "Persistence coordinator is unavailable"), {}};
}

std::optional<HistoryEntryDetail> PluginProcessor::inspectHistory(
    const juce::String& historyId)
{
    if (persistenceCoordinator == nullptr)
        return std::nullopt;
    const auto recalled = persistenceCoordinator->recallHistory(historyId);
    if (!recalled.succeeded())
        return std::nullopt;
    return HistoryEntryDetail{historySummary(recalled.entry), recalled.entry.macroSnapshot,
                              static_cast<int>(recalled.entry.composition.clips.size()),
                              compositionNoteCount(recalled.entry.composition)};
}

juce::Result PluginProcessor::recallHistory(const juce::String& historyId)
{
    if (persistenceCoordinator == nullptr)
        return juce::Result::fail("Persistence coordinator is unavailable");
    const auto recalled = persistenceCoordinator->recallHistory(historyId);
    if (!recalled.succeeded())
        return recalled.status;
    if (!recalled.entry.presetId.isEmpty())
    {
        if (const auto preset = loadLibraryPreset(recalled.entry.presetId); preset.failed())
            return juce::Result::fail("History preset could not be restored: "
                                      + preset.getErrorMessage());
    }
    if (const auto restored = compositionSession.restoreAccepted(recalled.entry.composition);
        restored.failed())
        return restored;
    {
        const std::lock_guard lock(historyLineageMutex);
        lastHistoryEntryId = recalled.entry.id;
        lastHistoryClipIds.clear();
        for (const auto& clip : recalled.entry.composition.clips)
            lastHistoryClipIds.push_back(clip.id);
    }
    persistenceCoordinator->report("History entry recalled exactly and explicitly accepted");
    return juce::Result::ok();
}

juce::Result PluginProcessor::setHistoryFavorite(const juce::String& historyId,
                                                 bool favorite)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->setHistoryFavorite(historyId, favorite)
        : juce::Result::fail("Persistence coordinator is unavailable");
}

juce::Result PluginProcessor::setHistorySoftDeleted(const juce::String& historyId,
                                                    bool deleted)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->setHistorySoftDeleted(historyId, deleted)
        : juce::Result::fail("Persistence coordinator is unavailable");
}

juce::Result PluginProcessor::setHistoryRetentionDays(int days)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->setRetentionDays(days)
        : juce::Result::fail("Persistence coordinator is unavailable");
}

persistence::HistoryCleanupResult PluginProcessor::cleanupHistory(bool keepFavorites)
{
    return persistenceCoordinator != nullptr
        ? persistenceCoordinator->cleanupHistory(juce::Time::currentTimeMillis(), keepFavorites)
        : persistence::HistoryCleanupResult{juce::Result::fail(
              "Persistence coordinator is unavailable"), 0};
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const std::lock_guard assistantLock(assistantAuditionMutex);
    auto snapshot = parameters.copyState();
    if (const auto existingRoutes = snapshot.getChildWithName(modulationRoutesType); existingRoutes.isValid())
        snapshot.removeChild(existingRoutes, nullptr);
    if (const auto existingSession = snapshot.getChildWithName(projectSessionType);
        existingSession.isValid())
        snapshot.removeChild(existingSession, nullptr);
    snapshot.appendChild(serialiseModulationRoutes(getConfiguredModulationRoutes()), nullptr);
    snapshot.setProperty("schemaVersion", 1, nullptr);
    snapshot.setProperty("productVersion", FOLK_PARK_VERSION, nullptr);

    const auto persistenceStatus = getPersistenceStatus();
    persistence::PresetMetadata metadata;
    metadata.id = midi::isUuid(persistenceStatus.currentPresetId)
        ? persistenceStatus.currentPresetId
        : projectSnapshotId;
    metadata.name = persistenceStatus.currentPresetName.trim().substring(0, 96);
    if (metadata.name.isEmpty())
        metadata.name = "Project state";
    const auto encodedPreset = persistence::PresetCodec::encode(captureCurrentPreset(metadata));
    if (encodedPreset.succeeded())
    {
        juce::ValueTree session(projectSessionType);
        session.setProperty("schemaVersion", projectSessionSchemaVersion, nullptr);
        session.setProperty("presetPayload", binaryPayload(encodedPreset.canonicalJson), nullptr);
        session.setProperty("currentPresetDirty", persistenceStatus.currentPresetDirty, nullptr);
        if (const auto accepted = compositionSession.getAcceptedBundle(); accepted.has_value())
        {
            const auto encodedComposition = persistence::encodeCompositionJson(*accepted);
            if (encodedComposition.succeeded())
                session.setProperty("acceptedCompositionPayload",
                                    binaryPayload(encodedComposition.json), nullptr);
        }
        {
            const std::lock_guard lock(historyLineageMutex);
            if (midi::isUuid(lastHistoryEntryId))
                session.setProperty("historyEntryId", lastHistoryEntryId, nullptr);
        }
        const auto assistantSnapshot = assistantAudition.snapshot();
        if (assistantSnapshot.active())
            session.appendChild(
                assistant::serialiseAssistantAudition(assistantSnapshot), nullptr);
        snapshot.appendChild(session, nullptr);
    }
    if (const auto xml = snapshot.createXml())
    {
        copyXmlToBinary(*xml, destination);
        if (destination.getSize() > static_cast<std::size_t>(maximumPluginStateBytes))
            destination.reset();
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    struct RejectionTally
    {
        std::atomic<std::uint64_t>& counter;
        bool accepted = false;
        ~RejectionTally()
        {
            if (!accepted)
                counter.fetch_add(1, std::memory_order_relaxed);
        }
    } rejection{rejectedProjectStates};

    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > maximumPluginStateBytes)
        return;
    const auto candidateXml = getXmlFromBinary(data, sizeInBytes);
    if (candidateXml == nullptr)
        return;
    const auto candidate = juce::ValueTree::fromXml(*candidateXml);
    if (!candidate.isValid() || candidate.getType() != parameters.state.getType())
        return;
    const auto schemaVersion = static_cast<int>(candidate.getProperty("schemaVersion", 1));
    if (schemaVersion != 1)
        return;

    juce::ValueTree projectSession;
    auto sessionCount = 0;
    for (int index = 0; index < candidate.getNumChildren(); ++index)
    {
        const auto child = candidate.getChild(index);
        if (child.hasType(projectSessionType))
        {
            projectSession = child;
            ++sessionCount;
        }
    }
    if (sessionCount > 1)
        return;
    if (projectSession.isValid())
    {
        int sessionVersion = 0;
        bool dirty = false;
        if (!hasOnlyProjectSessionProperties(projectSession)
            || !projectSession.hasProperty("schemaVersion")
            || !projectSession.hasProperty("presetPayload")
            || !projectSession.hasProperty("currentPresetDirty")
            || !parseInteger(projectSession["schemaVersion"], sessionVersion)
            || sessionVersion < oldestProjectSessionSchemaVersion
            || sessionVersion > projectSessionSchemaVersion
            || !hasOnlyProjectSessionChildren(projectSession, sessionVersion)
            || !parseBoolean(projectSession["currentPresetDirty"], dirty)
            || persistenceCoordinator == nullptr)
            return;

        std::optional<assistant::AssistantAuditionSnapshot> assistantSnapshot;
        if (sessionVersion >= 2 && projectSession.getNumChildren() == 1)
        {
            assistantSnapshot.emplace();
            if (assistant::parseAssistantAudition(projectSession.getChild(0),
                                                  *assistantSnapshot).failed())
                return;
        }

        juce::String presetJson;
        if (!readBinaryPayload(projectSession["presetPayload"],
                               persistence::maximumPresetBytes, presetJson))
            return;

        std::optional<midi::CompositionBundle> acceptedBundle;
        if (projectSession.hasProperty("acceptedCompositionPayload"))
        {
            juce::String compositionJson;
            if (!readBinaryPayload(projectSession["acceptedCompositionPayload"],
                                   persistence::maximumHistoryPayloadBytes, compositionJson))
                return;
            auto decoded = persistence::decodeCompositionJson(compositionJson);
            if (!decoded.succeeded())
                return;
            acceptedBundle = std::move(decoded.bundle);
        }
        juce::String historyEntryId;
        if (projectSession.hasProperty("historyEntryId"))
        {
            const auto value = projectSession["historyEntryId"];
            if (!value.isString() || !midi::isUuid(value.toString()))
                return;
            historyEntryId = value.toString();
        }

        const auto presetCandidate = persistenceCoordinator->prepareSessionPresetJson(presetJson);
        if (presetCandidate.status.failed())
        {
            clearPendingProjectRestore();
            return;
        }
        if (assistantSnapshot.has_value())
        {
            assistant::AssistantAuditionSession validationSession;
            const auto presetValues = assistantValuesForPreset(presetCandidate.document);
            if (validationSession.restore(*assistantSnapshot, presetValues).failed())
            {
                clearPendingProjectRestore();
                return;
            }
        }

        if (!presetCandidate.readyToApply())
        {
            const std::lock_guard lock(projectStateMutex);
            pendingProjectRestore = PendingProjectRestore{
                std::move(acceptedBundle), std::move(assistantSnapshot), dirty, historyEntryId};
            rejection.accepted = true;
            return;
        }
        clearPendingProjectRestore();
        if (applyPresetCandidate(presetCandidate).failed())
            return;
        if (completeProjectRestore(presetCandidate.document, std::move(acceptedBundle),
                                   std::move(assistantSnapshot), dirty,
                                   historyEntryId).failed())
            return;
        rejection.accepted = true;
        return;
    }

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
    (void) compositionSession.restoreProjectState(std::nullopt);
    {
        const std::lock_guard lock(historyLineageMutex);
        lastHistoryEntryId.clear();
        lastHistoryClipIds.clear();
    }
    cleanParameterRevision.store(parameterRevision.load(std::memory_order_acquire),
                                 std::memory_order_release);
    resetAssistantAudition();
    clearPendingProjectRestore();
    rejection.accepted = true;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new folkpark::PluginProcessor();
}
