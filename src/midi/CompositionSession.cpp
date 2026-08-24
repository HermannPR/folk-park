#include "CompositionSession.h"

#include <algorithm>
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
    candidateMatchesAccepted = false;
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
    candidateMatchesAccepted = false;
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
    candidateMatchesAccepted = false;
    status = "Surprise candidate generated; review and explicitly accept before delivery";
    return juce::Result::ok();
}

juce::Result CompositionSession::adjustCandidateNote(std::size_t sourceIndex,
                                                     int pitchDelta,
                                                     std::int64_t startDeltaTicks,
                                                     std::int64_t durationDeltaTicks,
                                                     int velocityDelta)
{
    if (pitchDelta < -24 || pitchDelta > 24 || startDeltaTicks < -compositionPpq * 4
        || startDeltaTicks > compositionPpq * 4 || durationDeltaTicks < -compositionPpq * 4
        || durationDeltaTicks > compositionPpq * 4 || velocityDelta < -127
        || velocityDelta > 127)
        return juce::Result::fail("Candidate note adjustment exceeds the bounded edit range");

    const std::lock_guard lock(mutex);
    if (!candidate.has_value())
        return juce::Result::fail("Generate a candidate before editing a note");
    auto edited = *candidate;
    auto remaining = sourceIndex;
    GeneratedClip* targetClip = nullptr;
    NoteEvent* targetEvent = nullptr;
    for (auto& clip : edited.clips)
    {
        if (remaining < clip.events.size())
        {
            targetClip = &clip;
            targetEvent = &clip.events[remaining];
            break;
        }
        remaining -= clip.events.size();
    }
    if (targetClip == nullptr || targetEvent == nullptr)
        return juce::Result::fail("Candidate note index is outside the bounded preview");

    const auto low = edited.intent.constraints.lowestMidiNote;
    const auto high = edited.intent.constraints.highestMidiNote;
    targetEvent->pitch = juce::jlimit(low, high, targetEvent->pitch + pitchDelta);
    targetEvent->velocity = juce::jlimit(1, 127, targetEvent->velocity + velocityDelta);
    const auto newStart = juce::jlimit<std::int64_t>(0, targetClip->lengthTicks - 1,
                                                     targetEvent->startTick + startDeltaTicks);
    const auto newDuration = juce::jlimit<std::int64_t>(1, targetClip->lengthTicks - newStart,
        targetEvent->durationTicks + durationDeltaTicks);
    targetEvent->startTick = newStart;
    targetEvent->durationTicks = newDuration;
    std::stable_sort(targetClip->events.begin(), targetClip->events.end(),
        [](const NoteEvent& left, const NoteEvent& right)
        {
            return left.startTick < right.startTick
                || (left.startTick == right.startTick && left.pitch < right.pitch);
        });
    if (const auto validation = validateBundle(edited); validation.failed())
        return juce::Result::fail("Candidate edit rejected: " + validation.getErrorMessage());
    candidate = std::move(edited);
    candidateMatchesAccepted = false;
    status = "Candidate note edited; review again and explicitly accept before delivery";
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
    candidateMatchesAccepted = true;
    status = "Candidate accepted; MIDI drag, export, and direct routing are enabled";
    return juce::Result::ok();
}

juce::Result CompositionSession::restoreAccepted(CompositionBundle bundle)
{
    return restoreProjectState(std::move(bundle));
}

juce::Result CompositionSession::restoreProjectState(
    std::optional<CompositionBundle> acceptedBundle)
{
    if (acceptedBundle.has_value())
    {
        if (const auto validation = validateBundle(*acceptedBundle); validation.failed())
            return juce::Result::fail("Recalled composition is invalid: "
                                      + validation.getErrorMessage());
    }
    const std::lock_guard lock(mutex);
    candidate = acceptedBundle;
    accepted = std::move(acceptedBundle);
    candidateMatchesAccepted = accepted.has_value();
    status = accepted.has_value()
        ? "Stored composition restored as accepted; delivery is enabled"
        : "No composition is stored in this project state";
    return juce::Result::ok();
}

void CompositionSession::clearCandidate()
{
    const std::lock_guard lock(mutex);
    candidate.reset();
    candidateMatchesAccepted = false;
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
    snapshot.candidateMatchesAccepted = candidate.has_value() && accepted.has_value()
        && candidateMatchesAccepted;
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
