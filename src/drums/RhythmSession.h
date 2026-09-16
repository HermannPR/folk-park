#pragma once

#include "RhythmGenerator.h"

#include <mutex>
#include <optional>

namespace folkpark::drums
{
struct RhythmSessionSnapshot
{
    bool hasCandidate = false;
    bool hasAccepted = false;
    bool candidateMatchesAccepted = false;
    juce::String candidateId;
    juce::String acceptedId;
    int candidateEventCount = 0;
    int acceptedEventCount = 0;
    juce::String status{"No rhythm has been generated"};
};

class RhythmSession final
{
public:
    [[nodiscard]] juce::Result generateCandidate(RhythmIntent intent);
    [[nodiscard]] juce::Result moreLikeCandidate(std::uint32_t variationIndex);
    [[nodiscard]] juce::Result acceptCandidate();
    void clearCandidate();

    [[nodiscard]] RhythmSessionSnapshot getSnapshot() const;
    [[nodiscard]] std::optional<DrumPattern> getCandidate() const;
    [[nodiscard]] std::optional<DrumPattern> getAccepted() const;

private:
    RhythmGenerator generator;
    mutable std::mutex mutex;
    std::optional<RhythmIntent> candidateIntent;
    std::optional<DrumPattern> candidate;
    std::optional<DrumPattern> accepted;
    bool candidateMatchesAccepted = false;
    juce::String status{"No rhythm has been generated"};
};
}
