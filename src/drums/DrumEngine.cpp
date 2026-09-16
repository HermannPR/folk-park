#include "DrumEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace folkpark::drums
{
namespace
{
constexpr double twoPi = 6.283185307179586476925286766559;

float articulationGain(DrumArticulation articulation) noexcept
{
    switch (articulation)
    {
        case DrumArticulation::accent: return 1.12f;
        case DrumArticulation::ghost: return 0.48f;
        case DrumArticulation::flam: return 0.82f;
        case DrumArticulation::normal:
        case DrumArticulation::count: break;
    }
    return 1.0f;
}

float articulationDecay(DrumArticulation articulation) noexcept
{
    if (articulation == DrumArticulation::ghost)
        return 0.72f;
    if (articulation == DrumArticulation::accent)
        return 1.08f;
    return 1.0f;
}

float lanePan(DrumLane lane) noexcept
{
    switch (lane)
    {
        case DrumLane::closedHat: return -0.18f;
        case DrumLane::openHat: return 0.18f;
        case DrumLane::percussion: return 0.28f;
        case DrumLane::kick:
        case DrumLane::snare:
        case DrumLane::count: break;
    }
    return 0.0f;
}
}

void DrumEngine::prepare(double newSampleRate, int maximumBlockSize,
                         const SynthDrumKit& newKit)
{
    juce::ignoreUnused(maximumBlockSize);
    sampleRate = std::isfinite(newSampleRate) && newSampleRate >= 8000.0
        ? newSampleRate : 48000.0;
    kit = validateSynthDrumKit(newKit).wasOk() ? newKit : SynthDrumKit{};
    reset();
}

bool DrumEngine::setKit(const SynthDrumKit& newKit) noexcept
{
    if (validateSynthDrumKit(newKit).failed())
        return false;
    kit = newKit;
    return true;
}

void DrumEngine::trigger(DrumLane lane, int velocity,
                         DrumArticulation articulation) noexcept
{
    if (static_cast<std::uint8_t>(lane) >= static_cast<std::uint8_t>(DrumLane::count)
        || static_cast<std::uint8_t>(articulation)
            >= static_cast<std::uint8_t>(DrumArticulation::count))
        return;

    if (lane == DrumLane::closedHat)
    {
        for (auto& voice : voices)
        {
            if (voice.active && voice.lane == DrumLane::openHat)
                voice.amplitudeMultiplier = std::min(voice.amplitudeMultiplier,
                                                      decayMultiplier(0.006f));
        }
    }

    auto& voice = selectVoice(lane);
    voice = {};
    voice.lane = lane;
    voice.active = true;
    voice.velocity = juce::jlimit(1, 127, velocity) / 127.0f
        * articulationGain(articulation);
    voice.amplitude = juce::jmin(1.2f, voice.velocity);
    voice.noiseState = static_cast<std::uint32_t>(0x9e3779b9U
        ^ (++triggerCounter * 0x85ebca6bULL)
        ^ (static_cast<std::uint64_t>(lane) * 0xc2b2ae35ULL));
    if (voice.noiseState == 0)
        voice.noiseState = 1;
    voice.age = triggerCounter;

    const auto decayScale = articulationDecay(articulation);
    switch (lane)
    {
        case DrumLane::kick:
            voice.amplitudeMultiplier = decayMultiplier(kit.kickDecaySeconds * decayScale);
            voice.pitchEnvelope = kit.kickTuneHz * 3.8f;
            voice.pitchMultiplier = decayMultiplier(0.028f);
            break;
        case DrumLane::snare:
            voice.amplitudeMultiplier = decayMultiplier(kit.snareDecaySeconds * decayScale);
            voice.pitchEnvelope = kit.snareTuneHz * 0.7f;
            voice.pitchMultiplier = decayMultiplier(0.045f);
            break;
        case DrumLane::closedHat:
            voice.amplitudeMultiplier = decayMultiplier(kit.closedHatDecaySeconds * decayScale);
            break;
        case DrumLane::openHat:
            voice.amplitudeMultiplier = decayMultiplier(kit.openHatDecaySeconds * decayScale);
            break;
        case DrumLane::percussion:
            voice.amplitudeMultiplier = decayMultiplier(kit.percussionDecaySeconds * decayScale);
            voice.pitchEnvelope = kit.percussionTuneHz * 0.9f;
            voice.pitchMultiplier = decayMultiplier(0.038f);
            break;
        case DrumLane::count:
            voice.active = false;
            break;
    }
}

void DrumEngine::process(juce::AudioBuffer<float>& output, int startSample,
                         int numberOfSamples) noexcept
{
    if (output.getNumChannels() == 0 || output.getNumSamples() == 0)
        return;
    startSample = juce::jlimit(0, output.getNumSamples(), startSample);
    if (numberOfSamples < 0)
        numberOfSamples = output.getNumSamples() - startSample;
    numberOfSamples = juce::jlimit(0, output.getNumSamples() - startSample,
                                  numberOfSamples);

    auto* left = output.getWritePointer(0, startSample);
    auto* right = output.getNumChannels() > 1
        ? output.getWritePointer(1, startSample) : left;
    const auto driveGain = 1.0f + kit.drive * 5.0f;
    const auto driveNorm = 1.0f / std::tanh(driveGain);

    for (int sampleIndex = 0; sampleIndex < numberOfSamples; ++sampleIndex)
    {
        auto drumLeft = 0.0f;
        auto drumRight = 0.0f;
        for (auto& voice : voices)
        {
            if (!voice.active)
                continue;
            const auto sample = renderVoice(voice);
            const auto pan = lanePan(voice.lane);
            drumLeft += sample * (1.0f - juce::jmax(0.0f, pan));
            drumRight += sample * (1.0f + juce::jmin(0.0f, pan));
        }
        drumLeft = std::tanh(drumLeft * driveGain) * driveNorm * kit.outputGain;
        drumRight = std::tanh(drumRight * driveGain) * driveNorm * kit.outputGain;
        if (!std::isfinite(drumLeft))
            drumLeft = 0.0f;
        if (!std::isfinite(drumRight))
            drumRight = 0.0f;
        left[sampleIndex] += drumLeft;
        if (right != left)
            right[sampleIndex] += drumRight;
    }
}

void DrumEngine::reset() noexcept
{
    for (auto& voice : voices)
        voice = {};
    triggerCounter = 0;
}

std::size_t DrumEngine::activeVoiceCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(voices.begin(), voices.end(),
        [](const Voice& voice) { return voice.active; }));
}

DrumEngine::Voice& DrumEngine::selectVoice(DrumLane lane) noexcept
{
    auto* selected = &voices[static_cast<std::size_t>(lane) * voicesPerLane];
    auto quietest = std::numeric_limits<float>::max();
    auto oldest = std::numeric_limits<std::uint64_t>::max();
    for (auto& voice : voices)
    {
        if (voice.lane != lane && voice.active)
            continue;
        if (!voice.active)
            return voice;
        if (voice.amplitude < quietest
            || (std::abs(voice.amplitude - quietest) < 0.0000001f
                && voice.age < oldest))
        {
            selected = &voice;
            quietest = voice.amplitude;
            oldest = voice.age;
        }
    }
    return *selected;
}

float DrumEngine::renderVoice(Voice& voice) noexcept
{
    auto sample = 0.0f;
    const auto noise = nextNoise(voice);
    switch (voice.lane)
    {
        case DrumLane::kick:
        {
            const auto frequency = kit.kickTuneHz + voice.pitchEnvelope;
            voice.phaseA += twoPi * frequency / sampleRate;
            if (voice.phaseA >= twoPi)
                voice.phaseA -= twoPi;
            const auto click = noise * kit.kickClick
                * voice.amplitude * voice.amplitude * voice.amplitude;
            sample = static_cast<float>(std::sin(voice.phaseA)) * voice.amplitude + click;
            voice.pitchEnvelope *= voice.pitchMultiplier;
            break;
        }
        case DrumLane::snare:
        {
            const auto frequency = kit.snareTuneHz + voice.pitchEnvelope;
            voice.phaseA += twoPi * frequency / sampleRate;
            voice.phaseB += twoPi * frequency * 1.47 / sampleRate;
            if (voice.phaseA >= twoPi) voice.phaseA -= twoPi;
            if (voice.phaseB >= twoPi) voice.phaseB -= twoPi;
            const auto body = 0.55f * static_cast<float>(std::sin(voice.phaseA))
                + 0.3f * static_cast<float>(std::sin(voice.phaseB));
            sample = ((1.0f - kit.snareNoise) * body + kit.snareNoise * noise)
                * voice.amplitude;
            voice.pitchEnvelope *= voice.pitchMultiplier;
            break;
        }
        case DrumLane::closedHat:
        case DrumLane::openHat:
        {
            const auto metal = kit.hatMetal;
            voice.phaseA += twoPi * (6123.0 + metal * 1377.0) / sampleRate;
            voice.phaseB += twoPi * (9471.0 + metal * 1731.0) / sampleRate;
            if (voice.phaseA >= twoPi) voice.phaseA -= twoPi;
            if (voice.phaseB >= twoPi) voice.phaseB -= twoPi;
            const auto oscillators = static_cast<float>(std::sin(voice.phaseA)
                + std::sin(voice.phaseB)) * 0.35f;
            const auto raw = noise * (0.55f + 0.35f * metal) + oscillators * metal;
            const auto highPassed = raw - voice.previousNoise * 0.92f;
            voice.previousNoise = raw;
            sample = highPassed * voice.amplitude;
            break;
        }
        case DrumLane::percussion:
        {
            const auto frequency = kit.percussionTuneHz + voice.pitchEnvelope;
            voice.phaseA += twoPi * frequency / sampleRate;
            voice.phaseB += twoPi * frequency * 1.93 / sampleRate;
            if (voice.phaseA >= twoPi) voice.phaseA -= twoPi;
            if (voice.phaseB >= twoPi) voice.phaseB -= twoPi;
            sample = (0.78f * static_cast<float>(std::sin(voice.phaseA))
                      + 0.22f * static_cast<float>(std::sin(voice.phaseB)))
                * voice.amplitude;
            voice.pitchEnvelope *= voice.pitchMultiplier;
            break;
        }
        case DrumLane::count:
            voice.active = false;
            break;
    }

    voice.amplitude *= voice.amplitudeMultiplier;
    if (!std::isfinite(sample) || !std::isfinite(voice.amplitude)
        || voice.amplitude < 0.00001f)
    {
        voice.active = false;
        voice.amplitude = 0.0f;
        return std::isfinite(sample) ? sample : 0.0f;
    }
    return sample;
}

float DrumEngine::nextNoise(Voice& voice) noexcept
{
    auto state = voice.noiseState;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    voice.noiseState = state == 0 ? 1 : state;
    return static_cast<float>(voice.noiseState & 0x00ffffffU)
        / static_cast<float>(0x00800000U) - 1.0f;
}

float DrumEngine::decayMultiplier(float seconds) const noexcept
{
    const auto samples = juce::jmax(1.0, static_cast<double>(seconds) * sampleRate);
    return static_cast<float>(std::exp(std::log(0.00001) / samples));
}
}
