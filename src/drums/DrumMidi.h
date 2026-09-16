#pragma once

#include "Rhythm.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace folkpark::drums
{
struct DrumMidiExportResult
{
    juce::Result status = juce::Result::fail("Drum MIDI export did not run");
    juce::MemoryBlock data;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk() && !data.isEmpty(); }
};

[[nodiscard]] int generalMidiPitch(DrumLane lane) noexcept;
[[nodiscard]] DrumMidiExportResult createDrumMidiFileData(const DrumPattern& pattern,
                                                          int targetPpq = rhythmPpq);
[[nodiscard]] juce::Result validateDrumMidiFileData(const juce::MemoryBlock& data,
                                                    const DrumPattern& source,
                                                    int expectedPpq = rhythmPpq);
}
