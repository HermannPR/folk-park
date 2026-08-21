#include "AssistantModels.h"

#include <array>
#include <cmath>
#include <set>

namespace folkpark::assistant
{
namespace
{
bool validText(const juce::String& text, int maximumLength) noexcept
{
    return text.length() <= maximumLength;
}

bool hasAnyAnswer(const SoundAnswers& answers) noexcept
{
    return !answers.musicalRole.trim().isEmpty() || !answers.timbre.trim().isEmpty()
        || !answers.articulation.trim().isEmpty() || !answers.movement.trim().isEmpty()
        || !answers.space.trim().isEmpty() || !answers.genreContext.trim().isEmpty()
        || !answers.referenceDescription.trim().isEmpty();
}
}

juce::String stableId(SoundEntryMode value)
{
    static constexpr std::array names{"describe", "guided", "manual"};
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? juce::String(names[index]) : juce::String{};
}

juce::Result validateSoundIntent(const SoundIntent& intent)
{
    if (intent.schemaVersion != SoundIntent::currentSchemaVersion)
        return juce::Result::fail("SoundIntent schema version is unsupported");
    if (!midi::isUuid(intent.requestId))
        return juce::Result::fail("SoundIntent requestId must be a UUID");
    if (stableId(intent.entryMode).isEmpty())
        return juce::Result::fail("SoundIntent entry mode is unsupported");
    const auto& answers = intent.answers;
    if (!validText(answers.musicalRole, 128) || !validText(answers.timbre, 256)
        || !validText(answers.articulation, 128) || !validText(answers.movement, 128)
        || !validText(answers.space, 128) || !validText(answers.genreContext, 128)
        || !validText(answers.referenceDescription, 512))
        return juce::Result::fail("SoundIntent contains an answer outside its length bound");
    if (!std::isfinite(answers.intensity) || answers.intensity < 0.0f || answers.intensity > 1.0f)
        return juce::Result::fail("SoundIntent intensity must be finite and normalized");
    if (intent.entryMode != SoundEntryMode::manual && !hasAnyAnswer(answers))
        return juce::Result::fail("Describe and guided modes require at least one producer answer");
    return juce::Result::ok();
}

juce::Result validateParameterProposal(const ParameterProposal& proposal)
{
    if (proposal.schemaVersion != ParameterProposal::currentSchemaVersion)
        return juce::Result::fail("ParameterProposal schema version is unsupported");
    if (!midi::isUuid(proposal.proposalId) || !midi::isUuid(proposal.requestId))
        return juce::Result::fail("ParameterProposal IDs must be UUIDs");
    if (proposal.changes.empty() || proposal.changes.size() > ParameterProposal::maximumChanges)
        return juce::Result::fail("ParameterProposal must contain 1-73 bounded changes");
    if (proposal.explanation.trim().isEmpty() || proposal.explanation.length() > 1024
        || proposal.assumptions.size() > 12)
        return juce::Result::fail("ParameterProposal explanation or assumptions are outside bounds");
    if (!std::isfinite(proposal.confidence) || proposal.confidence < 0.0f
        || proposal.confidence > 1.0f)
        return juce::Result::fail("ParameterProposal confidence must be finite and normalized");
    if (!proposal.requiresExplicitAcceptance)
        return juce::Result::fail("ParameterProposal cannot bypass explicit producer acceptance");

    std::set<juce::String> parameterIds;
    for (const auto& change : proposal.changes)
    {
        if (change.parameterId.isEmpty() || change.parameterId.length() > 64
            || change.reason.length() > 256)
            return juce::Result::fail("ParameterProposal contains invalid change text");
        if (!std::isfinite(change.currentNormalized) || change.currentNormalized < 0.0f
            || change.currentNormalized > 1.0f || !std::isfinite(change.proposedNormalized)
            || change.proposedNormalized < 0.0f || change.proposedNormalized > 1.0f)
            return juce::Result::fail("ParameterProposal change values must be finite and normalized");
        if (!parameterIds.insert(change.parameterId).second)
            return juce::Result::fail("ParameterProposal parameter IDs must be unique");
    }
    for (const auto& assumption : proposal.assumptions)
        if (assumption.length() > 256)
            return juce::Result::fail("ParameterProposal assumption is outside its length bound");
    return juce::Result::ok();
}
}
