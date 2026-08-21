#include "CompositionSession.h"

#include <limits>

namespace folkpark::midi
{
int CompositionSession::countNotes(const CompositionBundle& bundle) noexcept
{
    std::size_t count = 0;
    for (const auto& clip : bundle.clips)
        count += clip.events.size();
    return static_cast<int>(juce::jmin<std::size_t>(count,
        static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

std::int64_t CompositionSession::nowUnixMs() const noexcept
{
    return juce::Time::currentTimeMillis();
}

juce::Result CompositionSession::generateCandidate(MusicIntent intent)
{
    const auto result = engine.generate(std::move(intent), nowUnixMs());
    const std::lock_guard lock(mutex);
    if (!result.succeeded())
    {
        status = "Generation rejected: " + result.status.getErrorMessage();
        return result.status;
    }
    candidate = result.bundle;
    status = "Candidate generated; review and explicitly accept before delivery";
    return juce::Result::ok();
}

juce::Result CompositionSession::moreLikeCandidate(std::uint32_t variationIndex)
{
    CompositionBundle source;
    {
        const std::lock_guard lock(mutex);
        if (candidate.has_value())
            source = *candidate;
        else if (accepted.has_value())
            source = *accepted;
        else
            return juce::Result::fail("Generate a candidate before requesting More Like This");
    }
    const auto result = engine.moreLikeThis(source, variationIndex, nowUnixMs());
    const std::lock_guard lock(mutex);
    if (!result.succeeded())
    {
        status = "More Like This rejected: " + result.status.getErrorMessage();
        return result.status;
    }
    candidate = result.bundle;
    status = "Related candidate generated with parent lineage; acceptance is still required";
    return juce::Result::ok();
}

juce::Result CompositionSession::surpriseCandidate(std::uint32_t surpriseIndex)
{
    MusicIntent source;
    {
        const std::lock_guard lock(mutex);
        if (candidate.has_value())
            source = candidate->intent;
        else if (accepted.has_value())
            source = accepted->intent;
        else
            return juce::Result::fail("Generate a candidate before requesting Surprise Me");
    }
    const auto result = engine.surpriseMe(source, surpriseIndex, nowUnixMs());
    const std::lock_guard lock(mutex);
    if (!result.succeeded())
    {
        status = "Surprise Me rejected: " + result.status.getErrorMessage();
        return result.status;
    }
    candidate = result.bundle;
    status = "Surprise candidate generated; review and explicitly accept before delivery";
    return juce::Result::ok();
}

juce::Result CompositionSession::acceptCandidate()
{
    const std::lock_guard lock(mutex);
    if (!candidate.has_value())
        return juce::Result::fail("There is no candidate to accept");
    if (const auto validation = validateBundle(*candidate); validation.failed())
        return validation;
    accepted = *candidate;
    status = "Candidate accepted; MIDI drag, export, and direct routing are enabled";
    return juce::Result::ok();
}

void CompositionSession::clearCandidate()
{
    const std::lock_guard lock(mutex);
    candidate.reset();
    status = accepted.has_value() ? "Candidate cleared; accepted composition remains available"
                                  : "Candidate cleared";
}

CompositionSessionSnapshot CompositionSession::getSnapshot() const
{
    const std::lock_guard lock(mutex);
    CompositionSessionSnapshot snapshot;
    snapshot.status = status;
    snapshot.hasCandidate = candidate.has_value();
    snapshot.hasAccepted = accepted.has_value();
    if (candidate.has_value())
    {
        snapshot.candidateRequestId = candidate->intent.requestId;
        snapshot.candidateClipCount = static_cast<int>(candidate->clips.size());
        snapshot.candidateNoteCount = countNotes(*candidate);
    }
    if (accepted.has_value())
    {
        snapshot.acceptedRequestId = accepted->intent.requestId;
        snapshot.acceptedClipCount = static_cast<int>(accepted->clips.size());
        snapshot.acceptedNoteCount = countNotes(*accepted);
    }
    return snapshot;
}

PianoRollPreview CompositionSession::getCandidatePreview() const
{
    const std::lock_guard lock(mutex);
    return candidate.has_value() ? createPianoRollPreview(*candidate) : PianoRollPreview{};
}

std::optional<CompositionBundle> CompositionSession::getAcceptedBundle() const
{
    const std::lock_guard lock(mutex);
    return accepted;
}
}
