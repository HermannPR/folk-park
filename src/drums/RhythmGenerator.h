#pragma once

#include "Rhythm.h"

namespace folkpark::drums
{
struct RhythmGenerationResult
{
    juce::Result status = juce::Result::fail("Rhythm generation did not run");
    DrumPattern pattern;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

class RhythmGenerator final
{
public:
    [[nodiscard]] RhythmGenerationResult generate(RhythmIntent intent,
                                                  std::int64_t createdUnixMs = 0,
                                                  const juce::String& parentPatternId = {}) const;
    [[nodiscard]] RhythmGenerationResult moreLikeThis(const DrumPattern& source,
                                                      const RhythmIntent& sourceIntent,
                                                      std::uint32_t variationIndex,
                                                      std::int64_t createdUnixMs = 0) const;
};
}
