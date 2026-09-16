#include "drums/DrumEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool finite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
                return false;
    return true;
}

float magnitude(const juce::AudioBuffer<float>& buffer)
{
    auto peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    return peak;
}
}

int main()
{
    using namespace folkpark::drums;
    constexpr std::array sampleRates{44100.0, 48000.0, 96000.0};
    constexpr std::array blockSizes{32, 64, 128, 256, 512, 1024};

    for (const auto sampleRate : sampleRates)
    {
        for (const auto blockSize : blockSizes)
        {
            DrumEngine engine;
            juce::AudioBuffer<float> audio(2, blockSize);
            engine.prepare(sampleRate, blockSize);
            for (int lane = 0; lane < static_cast<int>(DrumLane::count); ++lane)
                engine.trigger(static_cast<DrumLane>(lane), 110, DrumArticulation::accent);
            audio.clear();
            engine.process(audio);
            expect(finite(audio), "Every supported drum render must remain finite");
            expect(magnitude(audio) > 0.0001f,
                   "Every supported configuration must render audible drum energy");
        }
    }

    DrumEngine first;
    DrumEngine second;
    juce::AudioBuffer<float> firstAudio(2, 512);
    juce::AudioBuffer<float> secondAudio(2, 512);
    first.prepare(48000.0, 512);
    second.prepare(48000.0, 512);
    firstAudio.clear();
    secondAudio.clear();
    for (const auto lane : {DrumLane::kick, DrumLane::snare, DrumLane::closedHat,
                            DrumLane::openHat, DrumLane::percussion})
    {
        first.trigger(lane, 100);
        second.trigger(lane, 100);
    }
    first.process(firstAudio);
    second.process(secondAudio);
    auto identical = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 512; ++sample)
            identical = identical
                && std::abs(firstAudio.getSample(channel, sample)
                            - secondAudio.getSample(channel, sample)) < 0.0000001f;
    expect(identical, "Synthesized drums must be deterministic from reset and triggers");

    first.trigger(DrumLane::openHat, 110);
    first.process(firstAudio);
    first.trigger(DrumLane::closedHat, 110);
    for (int block = 0; block < 2000 && first.activeVoiceCount() > 0; ++block)
    {
        firstAudio.clear();
        first.process(firstAudio);
        expect(finite(firstAudio), "Long synthesized drum tails must stay finite");
    }
    expect(first.activeVoiceCount() == 0,
           "Every synthesized drum voice must end after a bounded tail");

    first.trigger(DrumLane::kick, 127);
    first.reset();
    firstAudio.clear();
    first.process(firstAudio);
    expect(magnitude(firstAudio) < 0.0000001f,
           "Reset must silence all synthesized drum voices immediately");

    auto invalidKit = SynthDrumKit{};
    invalidKit.outputGain = std::nanf("");
    expect(!first.setKit(invalidKit),
           "Invalid drum kit updates must leave the valid engine unchanged");

    if (failures == 0)
    {
        std::cout << "Synthesized drum engine tests passed\n";
        return 0;
    }
    std::cerr << failures << " synthesized drum engine test(s) failed\n";
    return 1;
}
