#include "RhythmSession.h"

namespace folkpark::drums
{
juce::Result RhythmSession::generateCandidate(RhythmIntent intent)
{
    const auto generated = generator.generate(intent, juce::Time::currentTimeMillis());
    const std::lock_guard lock(mutex);
    if (!generated.succeeded())
    {
        status = "Rhythm generation rejected: " + generated.status.getErrorMessage();
        return generated.status;
    }
    candidateIntent = std::move(intent);
    candidate = generated.pattern;
    candidateMatchesAccepted = false;
    status = "Drum candidate generated; review and explicitly accept before delivery";
    return juce::Result::ok();
}

juce::Result RhythmSession::moreLikeCandidate(std::uint32_t variationIndex)
{
    DrumPattern source;
    RhythmIntent intent;
    {
        const std::lock_guard lock(mutex);
        if (!candidate.has_value() || !candidateIntent.has_value())
            return juce::Result::fail("Generate a drum candidate before requesting a variation");
        source = *candidate;
        intent = *candidateIntent;
    }
    const auto generated = generator.moreLikeThis(source, intent, variationIndex,
                                                   juce::Time::currentTimeMillis());
    const std::lock_guard lock(mutex);
    if (!generated.succeeded())
    {
        status = "Rhythm variation rejected: " + generated.status.getErrorMessage();
        return generated.status;
    }
    intent.seed = generated.pattern.seed;
    intent.requestId = generated.pattern.requestId;
    candidateIntent = std::move(intent);
    candidate = generated.pattern;
    candidateMatchesAccepted = false;
    status = "Related drum candidate generated; acceptance is still required";
    return juce::Result::ok();
}

juce::Result RhythmSession::acceptCandidate()
{
    const std::lock_guard lock(mutex);
    if (!candidate.has_value())
        return juce::Result::fail("There is no drum candidate to accept");
    if (const auto validation = validateDrumPattern(*candidate); validation.failed())
        return validation;
    accepted = *candidate;
    candidateMatchesAccepted = true;
    status = "Drum candidate accepted; MIDI and audio delivery are enabled";
    return juce::Result::ok();
}

void RhythmSession::clearCandidate()
{
    const std::lock_guard lock(mutex);
    candidate.reset();
    candidateIntent.reset();
    candidateMatchesAccepted = false;
    status = accepted.has_value() ? "Drum candidate cleared; accepted rhythm remains available"
                                  : "Drum candidate cleared";
}

RhythmSessionSnapshot RhythmSession::getSnapshot() const
{
    const std::lock_guard lock(mutex);
    RhythmSessionSnapshot snapshot;
    snapshot.hasCandidate = candidate.has_value();
    snapshot.hasAccepted = accepted.has_value();
    snapshot.candidateMatchesAccepted = candidateMatchesAccepted
        && candidate.has_value() && accepted.has_value();
    snapshot.status = status;
    if (candidate.has_value())
    {
        snapshot.candidateId = candidate->id;
        snapshot.candidateEventCount = static_cast<int>(candidate->events.size());
    }
    if (accepted.has_value())
    {
        snapshot.acceptedId = accepted->id;
        snapshot.acceptedEventCount = static_cast<int>(accepted->events.size());
    }
    return snapshot;
}

std::optional<DrumPattern> RhythmSession::getCandidate() const
{
    const std::lock_guard lock(mutex);
    return candidate;
}

std::optional<DrumPattern> RhythmSession::getAccepted() const
{
    const std::lock_guard lock(mutex);
    return accepted;
}
}
