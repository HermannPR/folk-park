#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace folkpark::midi
{
juce::MemoryBlock createM0ProofMidi();
juce::Result validateM0ProofMidi(const juce::MemoryBlock& data);
juce::File writeM0ProofMidiToTemporaryFile();
}
