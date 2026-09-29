#include "AssistantModels.h"

#include "common/ParameterIds.h"

#include <array>
#include <cmath>
#include <set>

namespace folkpark::assistant
{
static_assert(parameterIds::synthAndModulation.size() + parameterIds::allEffects.size()
              == ParameterProposal::maximumChanges);

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
        || !answers.referenceDescription.trim().isEmpty() || answers.intensity.has_value();
}

template <typename Values>
bool containsParameterId(const Values& values, const juce::String& candidate) noexcept
{
    for (const auto* value : values)
        if (candidate == value)
            return true;
    return false;
}
}

juce::String stableId(SoundEntryMode value)
{
    static constexpr std::array names{"describe", "guided", "manual"};
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? juce::String(names[index]) : juce::String{};
}

bool isKnownParameterId(const juce::String& parameterId, int proposalSchemaVersion) noexcept
{
    if (containsParameterId(parameterIds::synthAndModulation, parameterId))
        return true;
    return proposalSchemaVersion >= ParameterProposal::currentSchemaVersion
        && containsParameterId(parameterIds::allEffects, parameterId);
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
    if (answers.intensity.has_value()
        && (!std::isfinite(*answers.intensity) || *answers.intensity < 0.0f
            || *answers.intensity > 1.0f))
        return juce::Result::fail("SoundIntent intensity must be finite and normalized when provided");
    if (intent.entryMode != SoundEntryMode::manual && !hasAnyAnswer(answers))
        return juce::Result::fail("Describe and guided modes require at least one producer answer");
    return juce::Result::ok();
}

juce::Result validateParameterProposal(const ParameterProposal& proposal)
{
    if (proposal.schemaVersion < ParameterProposal::oldestSupportedSchemaVersion
        || proposal.schemaVersion > ParameterProposal::currentSchemaVersion)
        return juce::Result::fail("ParameterProposal schema version is unsupported");
    if (!midi::isUuid(proposal.proposalId) || !midi::isUuid(proposal.requestId))
        return juce::Result::fail("ParameterProposal IDs must be UUIDs");
    const auto maximumChanges = proposal.schemaVersion == 1
        ? ParameterProposal::maximumV1Changes : ParameterProposal::maximumChanges;
    if (proposal.changes.empty() || proposal.changes.size() > maximumChanges)
        return juce::Result::fail("ParameterProposal contains an unsupported number of changes");
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
            || change.reason.length() > 256
            || (proposal.schemaVersion >= 2 && change.reason.trim().isEmpty()))
            return juce::Result::fail("ParameterProposal contains invalid change text");
        if (!isKnownParameterId(change.parameterId, proposal.schemaVersion))
            return juce::Result::fail("ParameterProposal contains an unknown parameter ID");
        if (!std::isfinite(change.currentNormalized) || change.currentNormalized < 0.0f
            || change.currentNormalized > 1.0f || !std::isfinite(change.proposedNormalized)
            || change.proposedNormalized < 0.0f || change.proposedNormalized > 1.0f)
            return juce::Result::fail("ParameterProposal change values must be finite and normalized");
        if (!parameterIds.insert(change.parameterId).second)
            return juce::Result::fail("ParameterProposal parameter IDs must be unique");
    }
    for (const auto& assumption : proposal.assumptions)
        if (assumption.length() > 256
            || (proposal.schemaVersion >= 2 && assumption.trim().isEmpty()))
            return juce::Result::fail("ParameterProposal assumption is outside its length bound");
    return juce::Result::ok();
}

juce::Result validateCurrentParameterValues(std::span<const CurrentParameterValue> values)
{
    if (values.empty() || values.size() > ParameterProposal::maximumChanges)
        return juce::Result::fail("Assistant current-parameter snapshot is empty or oversized");
    std::set<juce::String> seen;
    for (const auto& value : values)
    {
        if (!isKnownParameterId(value.parameterId) || !std::isfinite(value.normalized)
            || value.normalized < 0.0f || value.normalized > 1.0f)
            return juce::Result::fail("Assistant current-parameter snapshot contains an invalid value");
        if (!seen.insert(value.parameterId).second)
            return juce::Result::fail("Assistant current-parameter snapshot contains a duplicate ID");
    }
    return juce::Result::ok();
}
}
