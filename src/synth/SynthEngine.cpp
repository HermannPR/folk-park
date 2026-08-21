#include "SynthEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace folkpark::synth
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

float decibelsToGain(float decibels) noexcept
{
    return decibels <= -60.0f ? 0.0f : std::pow(10.0f, decibels / 20.0f);
}

float midiNoteToFrequency(int midiNote, float pitchBendSemitones) noexcept
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f + pitchBendSemitones) / 12.0f);
}

float readTable(const std::array<float, SynthEngine::wavetableSize>& table, float phase) noexcept
{
    const auto tablePosition = phase * static_cast<float>(SynthEngine::wavetableSize);
    const auto firstIndex = static_cast<int>(tablePosition) % SynthEngine::wavetableSize;
    const auto secondIndex = (firstIndex + 1) % SynthEngine::wavetableSize;
    const auto fraction = tablePosition - static_cast<float>(firstIndex);
    return table[static_cast<std::size_t>(firstIndex)]
        + fraction * (table[static_cast<std::size_t>(secondIndex)]
                      - table[static_cast<std::size_t>(firstIndex)]);
}

void advancePhase(float& phase, float increment) noexcept
{
    phase += increment;
    if (phase >= 1.0f)
        phase -= std::floor(phase);
}
}

void SynthEngine::prepare(double newSampleRate, int maximumBlockSize) noexcept
{
    juce::ignoreUnused(maximumBlockSize);
    sampleRate = std::max(1.0, newSampleRate);
    initialiseWavetables();
    reset();
}

void SynthEngine::initialiseWavetables() noexcept
{
    for (int index = 0; index < wavetableSize; ++index)
    {
        const auto phase = static_cast<float>(index) / static_cast<float>(wavetableSize);
        wavetables[0][static_cast<std::size_t>(index)] = std::sin(twoPi * phase);
        wavetables[1][static_cast<std::size_t>(index)] = 1.0f - 4.0f * std::abs(phase - 0.5f);
    }
}

void SynthEngine::reset() noexcept
{
    for (auto& voice : voices)
        voice.reset();
    voiceAgeCounter = 0;
    pitchBendSemitones = 0.0f;
    sustainPedalDown = false;
    publishActiveVoiceCount();
}

void SynthEngine::panic() noexcept
{
    reset();
}

void SynthEngine::process(juce::AudioBuffer<float>& output,
                          const juce::MidiBuffer& midi,
                          const ParameterSnapshot& parameters) noexcept
{
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
                voice.release(parameters, true, sampleRate);
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
        if (voice.stage == EnvelopeStage::idle || voice.midiChannel != channel || voice.midiNote != note)
            continue;

        if (sustainPedalDown)
            voice.sustained = true;
        else
            voice.release(parameters, true, sampleRate);
    }
}

SynthEngine::Voice& SynthEngine::chooseVoiceToStart() noexcept
{
    for (auto& voice : voices)
    {
        if (voice.stage == EnvelopeStage::idle)
            return voice;
    }

    Voice* bestReleased = nullptr;
    for (auto& voice : voices)
    {
        if (voice.stage != EnvelopeStage::release)
            continue;
        if (bestReleased == nullptr
            || voice.envelopeLevel < bestReleased->envelopeLevel
            || (std::abs(voice.envelopeLevel - bestReleased->envelopeLevel) <= std::numeric_limits<float>::epsilon()
                && voice.startAge < bestReleased->startAge))
        {
            bestReleased = &voice;
        }
    }

    if (bestReleased != nullptr)
        return *bestReleased;

    return *std::min_element(voices.begin(), voices.end(), [](const Voice& left, const Voice& right)
    {
        return left.startAge < right.startAge;
    });
}

void SynthEngine::renderRange(juce::AudioBuffer<float>& output,
                              int startSample,
                              int endSample,
                              const ParameterSnapshot& parameters) noexcept
{
    if (endSample <= startSample)
        return;

    const auto waveformIndex = juce::jlimit(0, static_cast<int>(wavetables.size()) - 1, parameters.waveform);
    const auto& selectedTable = wavetables[static_cast<std::size_t>(waveformIndex)];
    const auto& sineTable = wavetables[0];
    const auto oscillatorGain = decibelsToGain(parameters.oscillatorLevelDb);
    const auto subGain = decibelsToGain(parameters.subLevelDb);
    const auto cutoff = juce::jlimit(20.0f,
                                    static_cast<float>(0.45 * sampleRate),
                                    parameters.filterCutoffHz);
    const auto lowPassCoefficient = 1.0f - std::exp(-twoPi * cutoff / static_cast<float>(sampleRate));
    const auto channelCount = juce::jmin(2, output.getNumChannels());

    for (int sample = startSample; sample < endSample; ++sample)
    {
        auto mixed = 0.0f;
        for (auto& voice : voices)
        {
            if (voice.stage != EnvelopeStage::idle)
            {
                mixed += voice.render(selectedTable,
                                      sineTable,
                                      oscillatorGain,
                                      subGain,
                                      lowPassCoefficient,
                                      pitchBendSemitones,
                                      sampleRate,
                                      parameters);
            }
        }

        for (int channel = 0; channel < channelCount; ++channel)
            output.setSample(channel, sample, mixed);
    }
}

void SynthEngine::publishActiveVoiceCount() noexcept
{
    auto count = 0;
    for (const auto& voice : voices)
        count += voice.stage == EnvelopeStage::idle ? 0 : 1;
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
        return voice.stage != EnvelopeStage::idle
            && voice.midiChannel == midiChannel
            && voice.midiNote == midiNote;
    });
}

void SynthEngine::Voice::start(int channel,
                               int note,
                               float noteVelocity,
                               std::uint64_t age,
                               double currentSampleRate,
                               const ParameterSnapshot& parameters) noexcept
{
    midiChannel = channel;
    midiNote = note;
    velocity = juce::jlimit(0.0f, 1.0f, noteVelocity);
    startAge = age;
    phase = 0.0f;
    subPhase = 0.0f;
    envelopeLevel = 0.0f;
    filterState = 0.0f;
    sustained = false;
    stage = EnvelopeStage::attack;
    updateEnvelopeRates(currentSampleRate, parameters);
}

void SynthEngine::Voice::release(const ParameterSnapshot& parameters,
                                 bool allowTail,
                                 double currentSampleRate) noexcept
{
    sustained = false;
    if (!allowTail || parameters.releaseSeconds <= 0.0f || envelopeLevel <= 0.0f)
    {
        reset();
        return;
    }

    stage = EnvelopeStage::release;
    releaseDecrement = envelopeLevel
        / std::max(1.0f, parameters.releaseSeconds * static_cast<float>(currentSampleRate));
}

void SynthEngine::Voice::reset() noexcept
{
    stage = EnvelopeStage::idle;
    midiChannel = 0;
    midiNote = -1;
    velocity = 0.0f;
    phase = 0.0f;
    subPhase = 0.0f;
    envelopeLevel = 0.0f;
    filterState = 0.0f;
    startAge = 0;
    sustained = false;
}

void SynthEngine::Voice::updateEnvelopeRates(double currentSampleRate,
                                              const ParameterSnapshot& parameters) noexcept
{
    const auto rate = static_cast<float>(std::max(1.0, currentSampleRate));
    attackIncrement = parameters.attackSeconds <= 0.0f
        ? 1.0f
        : 1.0f / std::max(1.0f, parameters.attackSeconds * rate);
    decayDecrement = parameters.decaySeconds <= 0.0f
        ? 1.0f
        : (1.0f - juce::jlimit(0.0f, 1.0f, parameters.sustainLevel))
            / std::max(1.0f, parameters.decaySeconds * rate);
    releaseDecrement = parameters.releaseSeconds <= 0.0f
        ? 1.0f
        : std::max(envelopeLevel, 1.0e-6f) / std::max(1.0f, parameters.releaseSeconds * rate);
}

float SynthEngine::Voice::nextEnvelopeSample(const ParameterSnapshot& parameters) noexcept
{
    const auto sustain = juce::jlimit(0.0f, 1.0f, parameters.sustainLevel);
    switch (stage)
    {
        case EnvelopeStage::idle:
            return 0.0f;
        case EnvelopeStage::attack:
            envelopeLevel += attackIncrement;
            if (envelopeLevel >= 1.0f)
            {
                envelopeLevel = 1.0f;
                stage = EnvelopeStage::decay;
            }
            break;
        case EnvelopeStage::decay:
            envelopeLevel -= decayDecrement;
            if (envelopeLevel <= sustain)
            {
                envelopeLevel = sustain;
                stage = EnvelopeStage::sustain;
            }
            break;
        case EnvelopeStage::sustain:
            envelopeLevel = sustain;
            break;
        case EnvelopeStage::release:
            envelopeLevel -= releaseDecrement;
            if (envelopeLevel <= 0.0f)
                reset();
            break;
    }
    return envelopeLevel;
}

float SynthEngine::Voice::render(const std::array<float, wavetableSize>& table,
                                 const std::array<float, wavetableSize>& sineTable,
                                 float oscillatorGain,
                                 float subGain,
                                 float lowPassCoefficient,
                                 float bendSemitones,
                                 double currentSampleRate,
                                 const ParameterSnapshot& parameters) noexcept
{
    const auto envelope = nextEnvelopeSample(parameters);
    if (stage == EnvelopeStage::idle)
        return 0.0f;

    const auto frequency = midiNoteToFrequency(midiNote, bendSemitones);
    const auto phaseIncrement = frequency / static_cast<float>(currentSampleRate);
    const auto subPhaseIncrement = phaseIncrement * 0.5f;
    const auto oscillator = readTable(table, phase) * oscillatorGain;
    const auto sub = readTable(sineTable, subPhase) * subGain;
    advancePhase(phase, phaseIncrement);
    advancePhase(subPhase, subPhaseIncrement);

    const auto input = (oscillator + sub) * velocity * envelope;
    filterState += lowPassCoefficient * (input - filterState);
    return std::isfinite(filterState) ? filterState : 0.0f;
}
}
