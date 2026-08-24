#include "SynthEngine.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace folkpark::synth
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr std::array<float, 7> syncCyclesPerBeat{0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};

const WavetableBank& builtInBank()
{
    static const auto bank = WavetableBank::createBuiltIn();
    return *bank;
}

float decibelsToGain(float decibels) noexcept
{
    return decibels <= -60.0f ? 0.0f : std::pow(10.0f, decibels / 20.0f);
}

float midiNoteToFrequency(float midiNote) noexcept
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

void advancePhase(float& phase, float increment) noexcept
{
    phase += increment;
    phase -= std::floor(phase);
}

float lfoRate(const LfoParameters& parameters, double tempoBpm) noexcept
{
    if (!parameters.tempoSync)
        return juce::jlimit(0.01f, 30.0f, parameters.rateHz);
    const auto division = juce::jlimit(0, static_cast<int>(syncCyclesPerBeat.size()) - 1,
                                      parameters.syncDivision);
    const auto beatsPerSecond = static_cast<float>(juce::jlimit(20.0, 400.0, tempoBpm) / 60.0);
    return beatsPerSecond * syncCyclesPerBeat[static_cast<std::size_t>(division)];
}

float lfoAt(float phase, LfoShape shape) noexcept
{
    const auto wrapped = phase - std::floor(phase);
    switch (shape)
    {
        case LfoShape::sine:
            return std::sin(twoPi * wrapped);
        case LfoShape::triangle:
            return 1.0f - 4.0f * std::abs(wrapped - 0.5f);
        case LfoShape::saw:
            return 2.0f * wrapped - 1.0f;
        case LfoShape::square:
            return wrapped < 0.5f ? 1.0f : -1.0f;
    }
    return 0.0f;
}

std::uint32_t xorshift(std::uint32_t& state) noexcept
{
    auto value = state == 0 ? 0x9e3779b9u : state;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    state = value;
    return value;
}

float randomUnipolar(std::uint32_t& state) noexcept
{
    return static_cast<float>(xorshift(state) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

float smoothToward(float current, float target, float coefficient) noexcept
{
    return current + coefficient * (target - current);
}
}

SynthEngine::SynthEngine()
    : oscillatorABank(builtInBank()), oscillatorBBank(builtInBank())
{
}

void SynthEngine::prepare(double newSampleRate, int maximumBlockSize) noexcept
{
    juce::ignoreUnused(maximumBlockSize);
    sampleRate = std::max(1.0, newSampleRate);
    smoothingCoefficient = 1.0f - std::exp(-1.0f / static_cast<float>(0.01 * sampleRate));
    reset();
}

void SynthEngine::reset() noexcept
{
    for (auto& voice : voices)
        voice.reset();
    globalLfoPhases.fill(0.0f);
    voiceAgeCounter = 0;
    pitchBendSemitones = 0.0f;
    modWheel = 0.0f;
    channelPressure = 0.0f;
    sustainPedalDown = false;
    publishActiveVoiceCount();
}

void SynthEngine::panic() noexcept
{
    reset();
}

bool SynthEngine::publishWavetable(int oscillatorIndex, const WavetableBank& bank) noexcept
{
    if ((oscillatorIndex != 0 && oscillatorIndex != 1)
        || !bank.isFiniteAndNormalised()
        || publicationWriter.test_and_set(std::memory_order_seq_cst))
        return false;
    publicationProducerActive.store(true, std::memory_order_seq_cst);
    const auto consumerBusy = publicationConsumerActive.load(std::memory_order_seq_cst);
    auto published = false;
    if (!consumerBusy)
        published = oscillatorIndex == 0 ? oscillatorABank.publish(bank)
                                         : oscillatorBBank.publish(bank);
    publicationProducerActive.store(false, std::memory_order_seq_cst);
    publicationWriter.clear(std::memory_order_seq_cst);
    return published;
}

bool SynthEngine::publishModulationRoutes(std::span<const ModulationRoute> routes) noexcept
{
    if (ModulationRegistry::validate(routes).failed()
        || publicationWriter.test_and_set(std::memory_order_seq_cst))
        return false;
    publicationProducerActive.store(true, std::memory_order_seq_cst);
    const auto consumerBusy = publicationConsumerActive.load(std::memory_order_seq_cst);
    const auto published = !consumerBusy && modulationExchange.publish(routes);
    publicationProducerActive.store(false, std::memory_order_seq_cst);
    publicationWriter.clear(std::memory_order_seq_cst);
    return published;
}

bool SynthEngine::publishPresetSnapshot(const WavetableBank& oscillatorA,
                                        const WavetableBank& oscillatorB,
                                        std::span<const ModulationRoute> routes) noexcept
{
    if (!oscillatorA.isFiniteAndNormalised() || !oscillatorB.isFiniteAndNormalised()
        || ModulationRegistry::validate(routes).failed()
        || publicationWriter.test_and_set(std::memory_order_seq_cst))
        return false;

    publicationProducerActive.store(true, std::memory_order_seq_cst);
    const auto unavailable = publicationConsumerActive.load(std::memory_order_seq_cst)
        || oscillatorABank.hasPendingBank() || oscillatorBBank.hasPendingBank()
        || modulationExchange.hasPendingSnapshot();
    auto published = false;
    if (!unavailable)
    {
        const auto publishedA = oscillatorABank.publish(oscillatorA);
        const auto publishedB = oscillatorBBank.publish(oscillatorB);
        const auto publishedRoutes = modulationExchange.publish(routes);
        jassert(publishedA && publishedB && publishedRoutes);
        published = publishedA && publishedB && publishedRoutes;
    }
    publicationProducerActive.store(false, std::memory_order_seq_cst);
    publicationWriter.clear(std::memory_order_seq_cst);
    return published;
}

const ModulationSnapshot& SynthEngine::getActiveModulationSnapshot() const noexcept
{
    return modulationExchange.current();
}

void SynthEngine::process(juce::AudioBuffer<float>& output,
                          const juce::MidiBuffer& midi,
                          const ParameterSnapshot& parameters) noexcept
{
    publicationConsumerActive.store(true, std::memory_order_seq_cst);
    if (!publicationProducerActive.load(std::memory_order_seq_cst))
    {
        oscillatorABank.beginAudioBlock();
        oscillatorBBank.beginAudioBlock();
        modulationExchange.beginAudioBlock();
    }
    publicationConsumerActive.store(false, std::memory_order_seq_cst);
    output.clear();
    const auto numberOfSamples = output.getNumSamples();
    auto renderedUntil = 0;

    for (const auto metadata : midi)
    {
        const auto eventSample = juce::jlimit(0, numberOfSamples, metadata.samplePosition);
        renderRange(output, renderedUntil, eventSample, parameters);
        handleMidiMessage(metadata.getMessage(), parameters);
        renderedUntil = eventSample;
    }

    renderRange(output, renderedUntil, numberOfSamples, parameters);
    publishActiveVoiceCount();
}

void SynthEngine::handleMidiMessage(const juce::MidiMessage& message,
                                    const ParameterSnapshot& parameters) noexcept
{
    if (message.isNoteOn())
    {
        startVoice(message.getChannel(), message.getNoteNumber(), message.getFloatVelocity(), parameters);
        return;
    }
    if (message.isNoteOff())
    {
        releaseVoices(message.getChannel(), message.getNoteNumber(), parameters);
        return;
    }
    if (message.isSustainPedalOn())
    {
        sustainPedalDown = true;
        return;
    }
    if (message.isSustainPedalOff())
    {
        sustainPedalDown = false;
        for (auto& voice : voices)
        {
            if (voice.sustained)
            {
                voice.sustained = false;
                voice.release(sampleRate, parameters, true);
            }
        }
        return;
    }
    if (message.isPitchWheel())
    {
        const auto normalised = (static_cast<float>(message.getPitchWheelValue()) - 8192.0f) / 8192.0f;
        pitchBendSemitones = juce::jlimit(-2.0f, 2.0f, normalised * 2.0f);
        return;
    }
    if (message.isController() && message.getControllerNumber() == 1)
    {
        modWheel = static_cast<float>(message.getControllerValue()) / 127.0f;
        return;
    }
    if (message.isChannelPressure())
    {
        channelPressure = static_cast<float>(message.getChannelPressureValue()) / 127.0f;
        return;
    }
    if (message.isAllNotesOff() || message.isAllSoundOff())
        panic();
}

void SynthEngine::startVoice(int channel,
                             int note,
                             float noteVelocity,
                             const ParameterSnapshot& parameters) noexcept
{
    auto& voice = chooseVoiceToStart();
    voice.start(channel, note, noteVelocity, ++voiceAgeCounter, sampleRate, parameters);
}

void SynthEngine::releaseVoices(int channel,
                                int note,
                                const ParameterSnapshot& parameters) noexcept
{
    for (auto& voice : voices)
    {
        if (voice.ampEnvelope.isIdle() || voice.midiChannel != channel || voice.midiNote != note)
            continue;
        if (sustainPedalDown)
            voice.sustained = true;
        else
            voice.release(sampleRate, parameters, true);
    }
}

SynthEngine::Voice& SynthEngine::chooseVoiceToStart() noexcept
{
    for (auto& voice : voices)
    {
        if (voice.ampEnvelope.isIdle())
            return voice;
    }

    voiceStealCount.fetch_add(1, std::memory_order_relaxed);

    Voice* bestReleased = nullptr;
    for (auto& voice : voices)
    {
        if (!voice.ampEnvelope.isReleased())
            continue;
        if (bestReleased == nullptr
            || voice.ampEnvelope.level < bestReleased->ampEnvelope.level
            || (std::abs(voice.ampEnvelope.level - bestReleased->ampEnvelope.level)
                    <= std::numeric_limits<float>::epsilon()
                && voice.startAge < bestReleased->startAge))
            bestReleased = &voice;
    }
    if (bestReleased != nullptr)
        return *bestReleased;

    return *std::min_element(voices.begin(), voices.end(), [](const Voice& left, const Voice& right)
    {
        return left.startAge < right.startAge;
    });
}

std::array<float, 4> SynthEngine::currentGlobalLfoValues(const ParameterSnapshot& parameters) const noexcept
{
    std::array<float, 4> values{};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        values[index] = lfoAt(globalLfoPhases[index] + parameters.lfos[index].phase,
                              parameters.lfos[index].shape);
    }
    return values;
}

void SynthEngine::advanceGlobalLfos(const ParameterSnapshot& parameters) noexcept
{
    for (std::size_t index = 0; index < globalLfoPhases.size(); ++index)
    {
        advancePhase(globalLfoPhases[index],
                     lfoRate(parameters.lfos[index], parameters.tempoBpm) / static_cast<float>(sampleRate));
    }
}

void SynthEngine::renderRange(juce::AudioBuffer<float>& output,
                              int startSample,
                              int endSample,
                              const ParameterSnapshot& parameters) noexcept
{
    if (endSample <= startSample)
        return;

    const auto channelCount = output.getNumChannels();
    for (int sample = startSample; sample < endSample; ++sample)
    {
        const auto oscillatorAView = oscillatorABank.renderView();
        const auto oscillatorBView = oscillatorBBank.renderView();
        const auto globalLfos = currentGlobalLfoValues(parameters);
        StereoSample mixed;

        for (auto& voice : voices)
        {
            if (!voice.ampEnvelope.isIdle())
            {
                const auto rendered = voice.render(oscillatorAView, oscillatorBView,
                                                   modulationExchange.current(), globalLfos,
                                                   pitchBendSemitones, modWheel, channelPressure,
                                                   sampleRate, smoothingCoefficient, parameters);
                mixed.left += rendered.left;
                mixed.right += rendered.right;
            }
        }

        if (channelCount > 0)
            output.setSample(0, sample, std::isfinite(mixed.left) ? mixed.left : 0.0f);
        if (channelCount > 1)
            output.setSample(1, sample, std::isfinite(mixed.right) ? mixed.right : 0.0f);
        oscillatorABank.advanceSample();
        oscillatorBBank.advanceSample();
        advanceGlobalLfos(parameters);
    }
}

void SynthEngine::publishActiveVoiceCount() noexcept
{
    auto count = 0;
    for (const auto& voice : voices)
        count += voice.ampEnvelope.isIdle() ? 0 : 1;
    activeVoiceCount.store(count, std::memory_order_relaxed);
}

int SynthEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount.load(std::memory_order_relaxed);
}

bool SynthEngine::isNoteActive(int midiChannel, int midiNote) const noexcept
{
    return std::any_of(voices.begin(), voices.end(), [midiChannel, midiNote](const Voice& voice)
    {
        return !voice.ampEnvelope.isIdle()
            && voice.midiChannel == midiChannel
            && voice.midiNote == midiNote;
    });
}

void SynthEngine::EnvelopeState::start(double currentSampleRate,
                                       const EnvelopeParameters& parameters) noexcept
{
    const auto rate = static_cast<float>(std::max(1.0, currentSampleRate));
    stage = EnvelopeStage::attack;
    level = 0.0f;
    attackIncrement = parameters.attackSeconds <= 0.0f
        ? 1.0f : 1.0f / std::max(1.0f, parameters.attackSeconds * rate);
    decayDecrement = parameters.decaySeconds <= 0.0f
        ? 1.0f
        : (1.0f - juce::jlimit(0.0f, 1.0f, parameters.sustainLevel))
            / std::max(1.0f, parameters.decaySeconds * rate);
    releaseDecrement = 1.0f;
}

void SynthEngine::EnvelopeState::release(double currentSampleRate,
                                         const EnvelopeParameters& parameters) noexcept
{
    if (stage == EnvelopeStage::idle)
        return;
    if (parameters.releaseSeconds <= 0.0f || level <= 0.0f)
    {
        reset();
        return;
    }
    stage = EnvelopeStage::release;
    releaseDecrement = level
        / std::max(1.0f, parameters.releaseSeconds * static_cast<float>(std::max(1.0, currentSampleRate)));
}

void SynthEngine::EnvelopeState::reset() noexcept
{
    stage = EnvelopeStage::idle;
    level = 0.0f;
    attackIncrement = 1.0f;
    decayDecrement = 1.0f;
    releaseDecrement = 1.0f;
}

float SynthEngine::EnvelopeState::next(const EnvelopeParameters& parameters) noexcept
{
    const auto sustain = juce::jlimit(0.0f, 1.0f, parameters.sustainLevel);
    switch (stage)
    {
        case EnvelopeStage::idle:
            return 0.0f;
        case EnvelopeStage::attack:
            level += attackIncrement;
            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = EnvelopeStage::decay;
            }
            break;
        case EnvelopeStage::decay:
            level -= decayDecrement;
            if (level <= sustain)
            {
                level = sustain;
                stage = EnvelopeStage::sustain;
            }
            break;
        case EnvelopeStage::sustain:
            level = sustain;
            break;
        case EnvelopeStage::release:
            level -= releaseDecrement;
            if (level <= 0.0f)
                reset();
            break;
    }
    return level;
}

void SynthEngine::Voice::start(int channel,
                               int note,
                               float noteVelocity,
                               std::uint64_t age,
                               double currentSampleRate,
                               const ParameterSnapshot& parameters) noexcept
{
    reset();
    midiChannel = channel;
    midiNote = note;
    velocity = juce::jlimit(0.0f, 1.0f, noteVelocity);
    currentPitch = static_cast<float>(note);
    targetPitch = currentPitch;
    startAge = age;
    noiseState = static_cast<std::uint32_t>(0x9e3779b9u ^ static_cast<std::uint32_t>(note * 131)
                                            ^ static_cast<std::uint32_t>(age));

    const std::array oscillatorParameters{parameters.oscillatorA, parameters.oscillatorB};
    for (std::size_t oscillator = 0; oscillator < oscillatorPhases.size(); ++oscillator)
    {
        const auto unison = juce::jlimit(1, maximumUnisonVoices,
                                        oscillatorParameters[oscillator].unisonVoices);
        smoothedOscillatorPitchOffset[oscillator]
            = oscillatorParameters[oscillator].coarseSemitones
            + oscillatorParameters[oscillator].fineCents / 100.0f;
        smoothedOscillatorPan[oscillator] = oscillatorParameters[oscillator].pan;
        smoothedOscillatorDetune[oscillator] = oscillatorParameters[oscillator].unisonDetuneCents;
        smoothedOscillatorSpread[oscillator] = oscillatorParameters[oscillator].unisonSpread;
        smoothedOscillatorBlend[oscillator] = oscillatorParameters[oscillator].unisonBlend;
        for (int lane = 0; lane < maximumUnisonVoices; ++lane)
        {
            smoothedUnisonWeights[oscillator][static_cast<std::size_t>(lane)]
                = lane < unison ? 1.0f : 0.0f;
            smoothedUnisonLanePositions[oscillator][static_cast<std::size_t>(lane)]
                = unison == 1 ? 0.0f
                              : 2.0f * static_cast<float>(lane)
                                    / static_cast<float>(unison - 1) - 1.0f;
            auto laneSeed = noiseState ^ static_cast<std::uint32_t>((oscillator + 1) * 0x85ebca6bu)
                ^ (static_cast<std::uint32_t>(lane + 1) * 0xc2b2ae35u);
            const auto randomOffset = randomUnipolar(laneSeed)
                * juce::jlimit(0.0f, 1.0f, oscillatorParameters[oscillator].randomPhase);
            const auto freeOffset = oscillatorParameters[oscillator].phaseReset
                ? 0.0f : static_cast<float>(age * 0.6180339887498948);
            oscillatorPhases[oscillator][static_cast<std::size_t>(lane)]
                = oscillatorParameters[oscillator].phase + randomOffset + freeOffset;
            oscillatorPhases[oscillator][static_cast<std::size_t>(lane)]
                -= std::floor(oscillatorPhases[oscillator][static_cast<std::size_t>(lane)]);
        }
    }
    subPhase = parameters.oscillatorA.phase;
    for (std::size_t index = 0; index < lfoPhases.size(); ++index)
        lfoPhases[index] = parameters.lfos[index].phase;

    ampEnvelope.start(currentSampleRate, parameters.ampEnvelope);
    filterEnvelope.start(currentSampleRate, parameters.filterEnvelope);
    auxiliaryEnvelope.start(currentSampleRate, parameters.auxiliaryEnvelope);

    smoothedOscillatorAPosition = juce::jlimit(0.0f, 1.0f,
        parameters.oscillatorA.position + (parameters.legacyOscillatorAWaveform == 1 ? 1.0f / 3.0f : 0.0f));
    smoothedOscillatorBPosition = juce::jlimit(0.0f, 1.0f, parameters.oscillatorB.position);
    smoothedOscillatorALevelDb = parameters.oscillatorA.levelDb;
    smoothedOscillatorBLevelDb = parameters.oscillatorB.levelDb;
    smoothedSubLevelDb = parameters.subLevelDb;
    smoothedNoiseLevelDb = parameters.noiseLevelDb;
    smoothedFilterCutoffHz = parameters.filterCutoffHz;
    smoothedFilterResonance = parameters.filterResonance;
    smoothedFilterDriveDb = parameters.filterDriveDb;
    smoothedPan = 0.0f;
}

void SynthEngine::Voice::release(double currentSampleRate,
                                 const ParameterSnapshot& parameters,
                                 bool allowTail) noexcept
{
    sustained = false;
    if (!allowTail)
    {
        reset();
        return;
    }
    ampEnvelope.release(currentSampleRate, parameters.ampEnvelope);
    filterEnvelope.release(currentSampleRate, parameters.filterEnvelope);
    auxiliaryEnvelope.release(currentSampleRate, parameters.auxiliaryEnvelope);
    if (ampEnvelope.isIdle())
        reset();
}

void SynthEngine::Voice::reset() noexcept
{
    midiChannel = 0;
    midiNote = -1;
    velocity = 0.0f;
    currentPitch = 60.0f;
    targetPitch = 60.0f;
    startAge = 0;
    sustained = false;
    for (auto& oscillator : oscillatorPhases)
        oscillator.fill(0.0f);
    for (auto& notes : cachedOscillatorNotes)
        notes.fill(std::numeric_limits<float>::quiet_NaN());
    for (auto& frequencies : cachedOscillatorFrequencies)
        frequencies.fill(1.0f);
    for (auto& levels : cachedOscillatorMipLevels)
        levels.fill(0);
    subPhase = 0.0f;
    lfoPhases.fill(0.0f);
    noiseState = 1;
    pinkNoiseState = 0.0f;
    ampEnvelope.reset();
    filterEnvelope.reset();
    auxiliaryEnvelope.reset();
    filterStates = {};
    modulationCache.fill(0.0f);
    smoothedOscillatorPitchOffset.fill(0.0f);
    smoothedOscillatorPan.fill(0.0f);
    smoothedOscillatorDetune.fill(0.0f);
    smoothedOscillatorSpread.fill(0.0f);
    smoothedOscillatorBlend.fill(0.0f);
    for (auto& weights : smoothedUnisonWeights)
        weights.fill(0.0f);
    for (auto& positions : smoothedUnisonLanePositions)
        positions.fill(0.0f);
}

float SynthEngine::Voice::nextLfo(int index,
                                  const LfoParameters& parameters,
                                  float globalValue,
                                  double tempoBpm,
                                  double currentSampleRate) noexcept
{
    if (!parameters.retrigger)
        return globalValue;
    auto& phase = lfoPhases[static_cast<std::size_t>(index)];
    const auto value = lfoAt(phase, parameters.shape);
    advancePhase(phase, lfoRate(parameters, tempoBpm) / static_cast<float>(currentSampleRate));
    return value;
}

SynthEngine::StereoSample SynthEngine::Voice::renderOscillator(
    const WavetableExchange::RenderView& bank,
    int oscillatorIndex,
    const OscillatorParameters& parameters,
    float positionModulation,
    float pitchModulation,
    float levelModulationDb,
    float panModulation,
    float bendSemitones,
    double currentSampleRate) noexcept
{
    StereoSample result;
    if (bank.current == nullptr)
        return result;

    const auto oscillator = static_cast<std::size_t>(juce::jlimit(0, 1, oscillatorIndex));
    auto weightSum = 0.0f;
    for (const auto weight : smoothedUnisonWeights[oscillator])
        weightSum += weight;
    const auto gain = decibelsToGain(parameters.levelDb + levelModulationDb)
        / std::max(weightSum, 1.0e-6f);
    const auto position = juce::jlimit(0.0f, 1.0f, parameters.position + positionModulation);
    for (int lane = 0; lane < maximumUnisonVoices; ++lane)
    {
        const auto laneWeight = smoothedUnisonWeights[oscillator][static_cast<std::size_t>(lane)];
        if (laneWeight <= 1.0e-6f)
            continue;
        const auto lanePosition
            = smoothedUnisonLanePositions[oscillator][static_cast<std::size_t>(lane)];
        const auto detuneSemitones = lanePosition * parameters.unisonDetuneCents
            * juce::jlimit(0.0f, 1.0f, parameters.unisonBlend) / 100.0f;
        const auto note = currentPitch + parameters.coarseSemitones + parameters.fineCents / 100.0f
            + bendSemitones + pitchModulation + detuneSemitones;
        auto& cachedNote = cachedOscillatorNotes[oscillator][static_cast<std::size_t>(lane)];
        auto& frequency = cachedOscillatorFrequencies[oscillator][static_cast<std::size_t>(lane)];
        auto& mipLevel = cachedOscillatorMipLevels[oscillator][static_cast<std::size_t>(lane)];
        if (std::bit_cast<std::uint32_t>(note) != std::bit_cast<std::uint32_t>(cachedNote))
        {
            cachedNote = note;
            frequency = juce::jlimit(1.0f, static_cast<float>(0.45 * currentSampleRate),
                                     midiNoteToFrequency(note));
            mipLevel = bank.current->mipLevelForFrequency(frequency, currentSampleRate);
        }
        auto& phase = oscillatorPhases[static_cast<std::size_t>(oscillatorIndex)][static_cast<std::size_t>(lane)];
        const auto waveform = bank.read(position, phase, mipLevel) * gain * laneWeight;
        const auto pan = juce::jlimit(-1.0f, 1.0f, parameters.pan + panModulation
            + lanePosition * parameters.unisonSpread * parameters.unisonBlend);
        const auto leftGain = std::sqrt(0.5f * (1.0f - pan));
        const auto rightGain = std::sqrt(0.5f * (1.0f + pan));
        result.left += waveform * leftGain;
        result.right += waveform * rightGain;
        advancePhase(phase, frequency / static_cast<float>(currentSampleRate));
    }
    return result;
}

float SynthEngine::Voice::processFilter(float input,
                                        FilterState& state,
                                        FilterMode mode,
                                        float cutoff,
                                        float resonance,
                                        float driveDb,
                                        double currentSampleRate) noexcept
{
    auto driven = input;
    if (driveDb > 0.001f)
    {
        const auto drive = decibelsToGain(juce::jlimit(0.0f, 24.0f, driveDb));
        driven = std::tanh(input * drive) / std::tanh(drive);
    }
    const auto safeCutoff = juce::jlimit(20.0f, static_cast<float>(0.45 * currentSampleRate), cutoff);
    const auto g = std::tan(juce::MathConstants<float>::pi * safeCutoff
                            / static_cast<float>(currentSampleRate));
    const auto damping = 2.0f - 1.9f * juce::jlimit(0.0f, 1.0f, resonance);
    const auto a1 = 1.0f / (1.0f + g * (g + damping));
    const auto a2 = g * a1;
    const auto a3 = g * a2;
    const auto v3 = driven - state.integrator2;
    const auto bandPass = a1 * state.integrator1 + a2 * v3;
    const auto lowPass = state.integrator2 + a2 * state.integrator1 + a3 * v3;
    state.integrator1 = 2.0f * bandPass - state.integrator1;
    state.integrator2 = 2.0f * lowPass - state.integrator2;
    const auto highPass = driven - damping * bandPass - lowPass;

    auto output = lowPass;
    switch (mode)
    {
        case FilterMode::lowPass:
            output = lowPass;
            break;
        case FilterMode::highPass:
            output = highPass;
            break;
        case FilterMode::bandPass:
            output = bandPass;
            break;
    }
    if (std::isfinite(output) && std::isfinite(state.integrator1) && std::isfinite(state.integrator2))
        return output;
    state = {};
    return 0.0f;
}

SynthEngine::StereoSample SynthEngine::Voice::render(
    const WavetableExchange::RenderView& oscillatorA,
    const WavetableExchange::RenderView& oscillatorB,
    const ModulationSnapshot& routes,
    const std::array<float, 4>& globalLfoValues,
    float bendSemitones,
    float currentModWheel,
    float currentChannelPressure,
    double currentSampleRate,
    float smoothingAmount,
    const ParameterSnapshot& parameters) noexcept
{
    const auto amp = ampEnvelope.next(parameters.ampEnvelope);
    const auto filterEnv = filterEnvelope.next(parameters.filterEnvelope);
    const auto auxiliaryEnv = auxiliaryEnvelope.next(parameters.auxiliaryEnvelope);
    if (ampEnvelope.isIdle())
    {
        reset();
        return {};
    }

    ModulationRegistry::SourceValues sourceValues{};
    sourceValues[static_cast<std::size_t>(ModulationSource::filterEnvelope)] = filterEnv;
    sourceValues[static_cast<std::size_t>(ModulationSource::auxiliaryEnvelope)] = auxiliaryEnv;
    for (int index = 0; index < 4; ++index)
    {
        sourceValues[static_cast<std::size_t>(ModulationSource::lfo1) + static_cast<std::size_t>(index)]
            = nextLfo(index, parameters.lfos[static_cast<std::size_t>(index)],
                      globalLfoValues[static_cast<std::size_t>(index)], parameters.tempoBpm,
                      currentSampleRate);
    }
    sourceValues[static_cast<std::size_t>(ModulationSource::velocity)] = velocity;
    sourceValues[static_cast<std::size_t>(ModulationSource::note)]
        = static_cast<float>(juce::jlimit(0, 127, midiNote)) / 127.0f;
    sourceValues[static_cast<std::size_t>(ModulationSource::modWheel)] = currentModWheel;
    sourceValues[static_cast<std::size_t>(ModulationSource::channelPressure)] = currentChannelPressure;
    modulationCache = ModulationRegistry::evaluate(routes, sourceValues);

    const auto destination = [this](ModulationDestination value)
    {
        return modulationCache[static_cast<std::size_t>(value)];
    };
    const auto aLegacyOffset = parameters.legacyOscillatorAWaveform == 1 ? 1.0f / 3.0f : 0.0f;
    const auto targetAPosition = juce::jlimit(0.0f, 1.0f, parameters.oscillatorA.position + aLegacyOffset
        + destination(ModulationDestination::oscillatorAPosition));
    const auto targetBPosition = juce::jlimit(0.0f, 1.0f, parameters.oscillatorB.position
        + destination(ModulationDestination::oscillatorBPosition));
    smoothedOscillatorAPosition = smoothToward(smoothedOscillatorAPosition, targetAPosition,
                                              smoothingAmount);
    smoothedOscillatorBPosition = smoothToward(smoothedOscillatorBPosition, targetBPosition,
                                              smoothingAmount);
    smoothedOscillatorALevelDb = smoothToward(smoothedOscillatorALevelDb,
        parameters.oscillatorA.levelDb + 24.0f * destination(ModulationDestination::oscillatorALevel),
        smoothingAmount);
    smoothedOscillatorBLevelDb = smoothToward(smoothedOscillatorBLevelDb,
        parameters.oscillatorB.levelDb + 24.0f * destination(ModulationDestination::oscillatorBLevel),
        smoothingAmount);
    smoothedSubLevelDb = smoothToward(smoothedSubLevelDb,
        parameters.subLevelDb + 24.0f * destination(ModulationDestination::subLevel), smoothingAmount);
    smoothedNoiseLevelDb = smoothToward(smoothedNoiseLevelDb,
        parameters.noiseLevelDb + 24.0f * destination(ModulationDestination::noiseLevel), smoothingAmount);
    smoothedPan = smoothToward(smoothedPan, destination(ModulationDestination::pan), smoothingAmount);

    auto oscillatorAParameters = parameters.oscillatorA;
    oscillatorAParameters.position = smoothedOscillatorAPosition;
    oscillatorAParameters.levelDb = smoothedOscillatorALevelDb;
    auto oscillatorBParameters = parameters.oscillatorB;
    oscillatorBParameters.position = smoothedOscillatorBPosition;
    oscillatorBParameters.levelDb = smoothedOscillatorBLevelDb;
    auto prepareSmoothedOscillator = [this, smoothingAmount](int oscillatorIndex,
                                                             OscillatorParameters& oscillator,
                                                             float pitchModulation)
    {
        const auto index = static_cast<std::size_t>(oscillatorIndex);
        const auto pitchTarget = oscillator.coarseSemitones + oscillator.fineCents / 100.0f
            + pitchModulation;
        smoothedOscillatorPitchOffset[index] = smoothToward(
            smoothedOscillatorPitchOffset[index], pitchTarget, smoothingAmount);
        smoothedOscillatorPan[index] = smoothToward(
            smoothedOscillatorPan[index], oscillator.pan, smoothingAmount);
        smoothedOscillatorDetune[index] = smoothToward(
            smoothedOscillatorDetune[index], oscillator.unisonDetuneCents, smoothingAmount);
        smoothedOscillatorSpread[index] = smoothToward(
            smoothedOscillatorSpread[index], oscillator.unisonSpread, smoothingAmount);
        smoothedOscillatorBlend[index] = smoothToward(
            smoothedOscillatorBlend[index], oscillator.unisonBlend, smoothingAmount);

        const auto targetUnison = juce::jlimit(1, maximumUnisonVoices, oscillator.unisonVoices);
        for (int lane = 0; lane < maximumUnisonVoices; ++lane)
        {
            auto& weight = smoothedUnisonWeights[index][static_cast<std::size_t>(lane)];
            const auto targetWeight = lane < targetUnison ? 1.0f : 0.0f;
            weight = smoothToward(weight, targetWeight, smoothingAmount);
            if (lane < targetUnison)
            {
                const auto targetPosition = targetUnison == 1 ? 0.0f
                    : 2.0f * static_cast<float>(lane) / static_cast<float>(targetUnison - 1) - 1.0f;
                auto& position = smoothedUnisonLanePositions[index][static_cast<std::size_t>(lane)];
                position = smoothToward(position, targetPosition, smoothingAmount);
            }
        }
        oscillator.coarseSemitones = smoothedOscillatorPitchOffset[index];
        oscillator.fineCents = 0.0f;
        oscillator.pan = smoothedOscillatorPan[index];
        oscillator.unisonDetuneCents = smoothedOscillatorDetune[index];
        oscillator.unisonSpread = smoothedOscillatorSpread[index];
        oscillator.unisonBlend = smoothedOscillatorBlend[index];
    };
    prepareSmoothedOscillator(0, oscillatorAParameters,
        24.0f * destination(ModulationDestination::oscillatorAPitch));
    prepareSmoothedOscillator(1, oscillatorBParameters,
        24.0f * destination(ModulationDestination::oscillatorBPitch));
    auto renderedA = renderOscillator(oscillatorA, 0, oscillatorAParameters, 0.0f,
        0.0f, 0.0f, smoothedPan,
        bendSemitones, currentSampleRate);
    const auto renderedB = renderOscillator(oscillatorB, 1, oscillatorBParameters, 0.0f,
        0.0f, 0.0f, smoothedPan,
        bendSemitones, currentSampleRate);
    renderedA.left += renderedB.left;
    renderedA.right += renderedB.right;

    const auto subFrequency = midiNoteToFrequency(currentPitch + bendSemitones
                                                   + 12.0f * static_cast<float>(parameters.subOctave));
    const auto subWave = parameters.subWaveform == 0
        ? std::sin(twoPi * subPhase)
        : 1.0f - 4.0f * std::abs(subPhase - 0.5f);
    const auto sub = subWave * decibelsToGain(smoothedSubLevelDb);
    advancePhase(subPhase, subFrequency / static_cast<float>(currentSampleRate));

    const auto white = 2.0f * randomUnipolar(noiseState) - 1.0f;
    pinkNoiseState = 0.98f * pinkNoiseState + 0.02f * white;
    const auto noiseWave = parameters.noiseType == 0 ? white : juce::jlimit(-1.0f, 1.0f, pinkNoiseState * 6.0f);
    const auto noise = noiseWave * decibelsToGain(smoothedNoiseLevelDb);
    const auto centreGain = std::sqrt(0.5f);
    renderedA.left += (sub + noise) * centreGain;
    renderedA.right += (sub + noise) * centreGain;

    const auto cutoffOctaves = parameters.filterEnvelopeOctaves * filterEnv
        + 8.0f * destination(ModulationDestination::filterCutoff)
        + parameters.filterKeyTracking * (currentPitch - 60.0f) / 12.0f;
    const auto cutoffTarget = juce::jlimit(20.0f, static_cast<float>(0.45 * currentSampleRate),
        parameters.filterCutoffHz * std::pow(2.0f, cutoffOctaves));
    smoothedFilterCutoffHz = smoothToward(smoothedFilterCutoffHz, cutoffTarget, smoothingAmount);
    smoothedFilterResonance = smoothToward(smoothedFilterResonance,
        juce::jlimit(0.0f, 1.0f, parameters.filterResonance
            + 0.95f * destination(ModulationDestination::filterResonance)), smoothingAmount);
    smoothedFilterDriveDb = smoothToward(smoothedFilterDriveDb,
        juce::jlimit(0.0f, 24.0f, parameters.filterDriveDb
            + 18.0f * destination(ModulationDestination::filterDrive)), smoothingAmount);

    const auto amplitudeModulation = juce::jlimit(0.0f, 2.0f,
        1.0f + destination(ModulationDestination::amplitude));
    StereoSample result;
    result.left = processFilter(renderedA.left * velocity * amp * amplitudeModulation,
                                filterStates[0], parameters.filterMode, smoothedFilterCutoffHz,
                                smoothedFilterResonance, smoothedFilterDriveDb, currentSampleRate);
    result.right = processFilter(renderedA.right * velocity * amp * amplitudeModulation,
                                 filterStates[1], parameters.filterMode, smoothedFilterCutoffHz,
                                 smoothedFilterResonance, smoothedFilterDriveDb, currentSampleRate);
    return result;
}
}
