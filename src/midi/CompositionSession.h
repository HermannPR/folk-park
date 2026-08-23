#pragma once

#include "Composition.h"

#include <mutex>
#include <optional>

namespace folkpark::midi
{
struct CompositionSessionSnapshot
{
    bool hasCandidate = false;
    bool hasAccepted = false;
    bool candidateMatchesAccepted = false;
    juce::String candidateRequestId;
    juce::String acceptedRequestId;
    juce::String status{"No composition has been generated"};
    int candidateClipCount = 0;
    int candidateNoteCount = 0;
    int acceptedClipCount = 0;
    int acceptedNoteCount = 0;
};

class CompositionSession final
{
public:
    [[nodiscard]] juce::Result generateCandidate(MusicIntent intent);
    [[nodiscard]] juce::Result moreLikeCandidate(std::uint32_t variationIndex);
    [[nodiscard]] juce::Result surpriseCandidate(std::uint32_t surpriseIndex);
    [[nodiscard]] juce::Result adjustCandidateNote(std::size_t sourceIndex,
                                                   int pitchDelta,
                                                   std::int64_t startDeltaTicks,
                                                   std::int64_t durationDeltaTicks,
                                                   int velocityDelta);
    [[nodiscard]] juce::Result acceptCandidate();
    [[nodiscard]] juce::Result restoreAccepted(CompositionBundle bundle);
    void clearCandidate();

    [[nodiscard]] CompositionSessionSnapshot getSnapshot() const;
    [[nodiscard]] PianoRollPreview getCandidatePreview() const;
    [[nodiscard]] std::optional<CompositionBundle> getAcceptedBundle() const;

private:
    static int countNotes(const CompositionBundle& bundle) noexcept;
    [[nodiscard]] std::int64_t nowUnixMs() const noexcept;

    CompositionEngine engine;
    mutable std::mutex mutex;
    std::optional<CompositionBundle> candidate;
    std::optional<CompositionBundle> accepted;
    bool candidateMatchesAccepted = false;
    juce::String status{"No composition has been generated"};
};
}
