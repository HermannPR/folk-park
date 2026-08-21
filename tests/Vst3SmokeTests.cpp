#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
bool isFinite(const juce::AudioBuffer<float>& audio)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            if (!std::isfinite(audio.getSample(channel, sample)))
                return false;
        }
    }
    return true;
}

float maximumMagnitude(const juce::AudioBuffer<float>& audio)
{
    auto magnitude = 0.0f;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        magnitude = std::max(magnitude, audio.getMagnitude(channel, 0, audio.getNumSamples()));
    return magnitude;
}

int fail(const juce::String& message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;
    if (argc != 2)
        return fail("Expected the built VST3 bundle path");

    const juce::String pluginPath(argv[1]);
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, pluginPath);
    if (descriptions.size() != 1)
        return fail("Expected exactly one plug-in type in " + pluginPath);

    juce::String creationError;
    auto instance = format.createInstanceFromDescription(*descriptions[0], 48000.0, 512, creationError);
    if (instance == nullptr)
        return fail("Could not instantiate built VST3: " + creationError);
    if (instance->getPluginDescription().pluginFormatName != "VST3")
        return fail("Loaded instance did not report VST3 format");
    if (instance->getTotalNumInputChannels() != 0 || instance->getTotalNumOutputChannels() != 2)
        return fail("Built VST3 must expose a zero-input, stereo-output instrument bus");
    if (instance->getTailLengthSeconds() < 10.0)
        return fail("Built VST3 must report its maximum envelope release tail");

    instance->setRateAndBufferSizeDetails(48000.0, 512);
    instance->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> audio(2, 512);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    instance->processBlock(audio, midi);

    if (!isFinite(audio))
        return fail("Built VST3 produced non-finite audio after MIDI note-on");
    if (maximumMagnitude(audio) <= 1.0e-6f)
        return fail("Built VST3 produced no audible output after MIDI note-on");
    if (std::abs(audio.getMagnitude(0, 0, 512) - audio.getMagnitude(1, 0, 512)) > 1.0e-7f)
        return fail("Built VST3 output was not centred stereo");

    instance->releaseResources();
    std::cout << "PASS: built x86_64 VST3 instantiated and rendered finite stereo audio from MIDI\n";
    return 0;
}
