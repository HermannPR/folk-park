#include "AssistantAudition.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace folkpark::assistant
{
namespace
{
constexpr float valueTolerance = 1.0e-6f;
const juce::Identifier auditionType{"FolkParkAssistantAudition"};
const juce::Identifier proposalType{"ParameterProposal"};
const juce::Identifier changeType{"ParameterChange"};
const juce::Identifier assumptionType{"Assumption"};

template <std::size_t Size>
bool hasOnlyProperties(const juce::ValueTree& tree,
                       const std::array<const char*, Size>& allowed)
{
    for (int index = 0; index < tree.getNumProperties(); ++index)
    {
        const auto name = tree.getPropertyName(index).toString();
        if (std::none_of(allowed.begin(), allowed.end(), [&name](const char* candidate)
                         { return name == candidate; }))
            return false;
    }
    return true;
}

bool parseInteger(const juce::var& value, int& output) noexcept
{
    if (value.isInt())
    {
        output = static_cast<int>(value);
        return true;
    }
    if (value.isInt64())
    {
        const auto parsed = static_cast<juce::int64>(value);
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
            return false;
        output = static_cast<int>(parsed);
        return true;
    }
    if (!value.isString())
        return false;
    const auto text = value.toString().trim();
    if (text.isEmpty())
        return false;
    const auto utf8 = text.toUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtol(utf8.getAddress(), &end, 10);
    if (errno != 0 || end == utf8.getAddress() || *end != '\0'
        || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
        return false;
    output = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const juce::var& value, float& output) noexcept
{
    if (value.isDouble() || value.isInt() || value.isInt64())
    {
        output = static_cast<float>(value);
        return std::isfinite(output);
    }
    if (!value.isString())
        return false;
    const auto text = value.toString().trim();
    if (text.isEmpty())
        return false;
    const auto utf8 = text.toUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtof(utf8.getAddress(), &end);
    if (errno != 0 || end == utf8.getAddress() || *end != '\0' || !std::isfinite(parsed))
        return false;
    output = parsed;
    return true;
}

bool parseBoolean(const juce::var& value, bool& output) noexcept
{
    if (value.isBool())
    {
        output = static_cast<bool>(value);
        return true;
    }
    int integer = 0;
    if (!parseInteger(value, integer) || (integer != 0 && integer != 1))
        return false;
    output = integer == 1;
    return true;
}

std::optional<AssistantSessionStatus> parseActiveStatus(const juce::String& value)
{
    if (value == stableId(AssistantSessionStatus::proposalReady))
        return AssistantSessionStatus::proposalReady;
    if (value == stableId(AssistantSessionStatus::previewing))
        return AssistantSessionStatus::previewing;
    return std::nullopt;
}

std::optional<AuditionSide> parseSide(const juce::String& value)
{
    if (value == stableId(AuditionSide::original)) return AuditionSide::original;
    if (value == stableId(AuditionSide::proposal)) return AuditionSide::proposal;
    return std::nullopt;
}

const CurrentParameterValue* findCurrent(
    std::span<const CurrentParameterValue> currentParameters,
    const juce::String& parameterId)
{
    for (const auto& value : currentParameters)
        if (value.parameterId == parameterId)
            return &value;
    return nullptr;
}

bool isActiveStatus(AssistantSessionStatus status) noexcept
{
    return status == AssistantSessionStatus::proposalReady
        || status == AssistantSessionStatus::previewing;
}
}

juce::String stableId(AuditionSide value)
{
    static constexpr std::array names{"original", "proposal"};
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? juce::String(names[index]) : juce::String{};
}

juce::Result AssistantAuditionSession::begin(
    const ParameterProposal& proposal,
    std::span<const CurrentParameterValue> currentParameters)
{
    if (state.active())
        return juce::Result::fail("An assistant A/B proposal is already active");
    if (const auto validation = validateParameterProposal(proposal); validation.failed())
        return validation;
    if (const auto validation = validateCurrentParameterValues(currentParameters);
        validation.failed())
        return validation;
    for (const auto& change : proposal.changes)
    {
        const auto* current = findCurrent(currentParameters, change.parameterId);
        if (current == nullptr
            || std::abs(current->normalized - change.currentNormalized) > valueTolerance)
            return juce::Result::fail("Assistant proposal is stale for the current sound");
        if (std::abs(change.currentNormalized - change.proposedNormalized) <= valueTolerance)
            return juce::Result::fail("Assistant proposal contains a no-op parameter change");
    }

    state.status = AssistantSessionStatus::proposalReady;
    state.audibleSide = AuditionSide::original;
    state.proposal = proposal;
    state.message = "Original A is active; proposal B is ready to audition";
    return juce::Result::ok();
}

juce::Result AssistantAuditionSession::restore(
    const AssistantAuditionSnapshot& snapshotToRestore,
    std::span<const CurrentParameterValue> currentParameters)
{
    if (!snapshotToRestore.active() || !isActiveStatus(snapshotToRestore.status)
        || stableId(snapshotToRestore.audibleSide).isEmpty())
        return juce::Result::fail("Assistant A/B restore snapshot is not active and valid");
    if (snapshotToRestore.status == AssistantSessionStatus::proposalReady
        && snapshotToRestore.audibleSide != AuditionSide::original)
        return juce::Result::fail("A ready assistant proposal must restore on original A");
    if (const auto validation = validateParameterProposal(*snapshotToRestore.proposal);
        validation.failed())
        return validation;

    const auto previous = state;
    state = snapshotToRestore;
    if (const auto validation = validateAudibleValues(currentParameters); validation.failed())
    {
        state = previous;
        return validation;
    }
    state.message = state.audibleSide == AuditionSide::original
        ? "Restored original A with proposal B available"
        : "Restored proposal B with original A available";
    return juce::Result::ok();
}

juce::Result AssistantAuditionSession::validateAudibleValues(
    std::span<const CurrentParameterValue> currentParameters)
{
    if (!state.active() || !state.proposal.has_value())
        return juce::Result::fail("No assistant A/B proposal is active");
    if (const auto validation = validateCurrentParameterValues(currentParameters);
        validation.failed())
        return validation;
    for (const auto& change : state.proposal->changes)
    {
        const auto* current = findCurrent(currentParameters, change.parameterId);
        const auto expected = state.audibleSide == AuditionSide::original
            ? change.currentNormalized : change.proposedNormalized;
        if (current == nullptr || std::abs(current->normalized - expected) > valueTolerance)
        {
            invalidate("Assistant A/B stopped because the sound changed outside the active proposal");
            return juce::Result::fail(state.message);
        }
    }
    return juce::Result::ok();
}

std::vector<CurrentParameterValue> AssistantAuditionSession::valuesFor(AuditionSide side) const
{
    std::vector<CurrentParameterValue> values;
    if (!state.proposal.has_value())
        return values;
    values.reserve(state.proposal->changes.size());
    for (const auto& change : state.proposal->changes)
        values.push_back({change.parameterId,
                          side == AuditionSide::original
                              ? change.currentNormalized : change.proposedNormalized});
    return values;
}

juce::Result AssistantAuditionSession::applySide(
    AuditionSide side,
    std::span<const CurrentParameterValue> currentParameters,
    const ApplyValues& apply,
    bool finish)
{
    if (stableId(side).isEmpty())
        return juce::Result::fail("Assistant A/B side is unsupported");
    if (const auto validation = validateAudibleValues(currentParameters); validation.failed())
        return validation;
    if (side != state.audibleSide)
    {
        if (!apply)
            return juce::Result::fail("Assistant A/B apply callback is unavailable");
        const auto values = valuesFor(side);
        apply(values);
        state.audibleSide = side;
    }
    if (!finish)
    {
        state.status = AssistantSessionStatus::previewing;
        state.message = side == AuditionSide::original
            ? "Auditioning original A" : "Auditioning proposal B";
    }
    return juce::Result::ok();
}

juce::Result AssistantAuditionSession::audition(
    AuditionSide side,
    std::span<const CurrentParameterValue> currentParameters,
    const ApplyValues& apply)
{
    return applySide(side, currentParameters, apply, false);
}

juce::Result AssistantAuditionSession::accept(
    std::span<const CurrentParameterValue> currentParameters,
    const ApplyValues& apply)
{
    if (const auto applied = applySide(AuditionSide::proposal, currentParameters, apply, true);
        applied.failed())
        return applied;
    state.status = AssistantSessionStatus::accepted;
    state.message = "Assistant proposal B was explicitly accepted";
    return juce::Result::ok();
}

juce::Result AssistantAuditionSession::reject(
    std::span<const CurrentParameterValue> currentParameters,
    const ApplyValues& apply)
{
    if (const auto applied = applySide(AuditionSide::original, currentParameters, apply, true);
        applied.failed())
        return applied;
    state.status = AssistantSessionStatus::rejected;
    state.message = "Assistant proposal B was rejected and original A was restored";
    return juce::Result::ok();
}

void AssistantAuditionSession::invalidate(const juce::String& reason)
{
    state.status = AssistantSessionStatus::failed;
    state.message = reason.trim().substring(0, AssistantResponse::maximumSummaryLength);
    if (state.message.isEmpty())
        state.message = "Assistant A/B session was invalidated";
}

void AssistantAuditionSession::reset()
{
    state = {};
}

juce::ValueTree serialiseAssistantAudition(const AssistantAuditionSnapshot& snapshot)
{
    if (!snapshot.active() || !snapshot.proposal.has_value())
        return {};
    juce::ValueTree root(auditionType);
    root.setProperty("schemaVersion", 1, nullptr);
    root.setProperty("status", stableId(snapshot.status), nullptr);
    root.setProperty("audibleSide", stableId(snapshot.audibleSide), nullptr);

    const auto& source = *snapshot.proposal;
    juce::ValueTree proposal(proposalType);
    proposal.setProperty("schemaVersion", source.schemaVersion, nullptr);
    proposal.setProperty("proposalId", source.proposalId, nullptr);
    proposal.setProperty("requestId", source.requestId, nullptr);
    proposal.setProperty("explanation", source.explanation, nullptr);
    proposal.setProperty("confidence", source.confidence, nullptr);
    proposal.setProperty("requiresExplicitAcceptance",
                         source.requiresExplicitAcceptance, nullptr);
    for (const auto& sourceChange : source.changes)
    {
        juce::ValueTree change(changeType);
        change.setProperty("parameterId", sourceChange.parameterId, nullptr);
        change.setProperty("currentNormalized", sourceChange.currentNormalized, nullptr);
        change.setProperty("proposedNormalized", sourceChange.proposedNormalized, nullptr);
        change.setProperty("reason", sourceChange.reason, nullptr);
        proposal.appendChild(change, nullptr);
    }
    for (const auto& sourceAssumption : source.assumptions)
    {
        juce::ValueTree assumption(assumptionType);
        assumption.setProperty("text", sourceAssumption, nullptr);
        proposal.appendChild(assumption, nullptr);
    }
    root.appendChild(proposal, nullptr);
    return root;
}

juce::Result parseAssistantAudition(const juce::ValueTree& tree,
                                    AssistantAuditionSnapshot& snapshot)
{
    constexpr std::array rootProperties{"schemaVersion", "status", "audibleSide"};
    constexpr std::array proposalProperties{
        "schemaVersion", "proposalId", "requestId", "explanation", "confidence",
        "requiresExplicitAcceptance"};
    constexpr std::array changeProperties{
        "parameterId", "currentNormalized", "proposedNormalized", "reason"};
    constexpr std::array assumptionProperties{"text"};
    if (!tree.hasType(auditionType) || !hasOnlyProperties(tree, rootProperties)
        || tree.getNumChildren() != 1 || !tree.hasProperty("schemaVersion")
        || !tree.hasProperty("status") || !tree.hasProperty("audibleSide"))
        return juce::Result::fail("Assistant A/B project state has an invalid root");
    int schemaVersion = 0;
    if (!parseInteger(tree["schemaVersion"], schemaVersion) || schemaVersion != 1
        || !tree["status"].isString() || !tree["audibleSide"].isString())
        return juce::Result::fail("Assistant A/B project state has unsupported root values");
    const auto status = parseActiveStatus(tree["status"].toString());
    const auto side = parseSide(tree["audibleSide"].toString());
    if (!status.has_value() || !side.has_value()
        || (*status == AssistantSessionStatus::proposalReady
            && *side != AuditionSide::original))
        return juce::Result::fail("Assistant A/B project state has an invalid active side");

    const auto proposalTree = tree.getChild(0);
    if (!proposalTree.hasType(proposalType)
        || !hasOnlyProperties(proposalTree, proposalProperties)
        || !proposalTree.hasProperty("schemaVersion")
        || !proposalTree.hasProperty("proposalId")
        || !proposalTree.hasProperty("requestId")
        || !proposalTree.hasProperty("explanation")
        || !proposalTree.hasProperty("confidence")
        || !proposalTree.hasProperty("requiresExplicitAcceptance")
        || !proposalTree["proposalId"].isString()
        || !proposalTree["requestId"].isString()
        || !proposalTree["explanation"].isString())
        return juce::Result::fail("Assistant A/B project proposal has invalid properties");

    ParameterProposal proposal;
    if (!parseInteger(proposalTree["schemaVersion"], proposal.schemaVersion)
        || !parseFloat(proposalTree["confidence"], proposal.confidence))
        return juce::Result::fail("Assistant A/B project proposal has invalid numeric values");
    proposal.proposalId = proposalTree["proposalId"].toString();
    proposal.requestId = proposalTree["requestId"].toString();
    proposal.explanation = proposalTree["explanation"].toString();
    if (!parseBoolean(proposalTree["requiresExplicitAcceptance"],
                      proposal.requiresExplicitAcceptance))
        return juce::Result::fail("Assistant A/B project proposal has an invalid acceptance flag");
    for (int index = 0; index < proposalTree.getNumChildren(); ++index)
    {
        const auto child = proposalTree.getChild(index);
        if (child.hasType(changeType))
        {
            if (!hasOnlyProperties(child, changeProperties)
                || child.getNumChildren() != 0 || !child.hasProperty("parameterId")
                || !child.hasProperty("currentNormalized")
                || !child.hasProperty("proposedNormalized")
                || !child.hasProperty("reason") || !child["parameterId"].isString()
                || !child["reason"].isString())
                return juce::Result::fail("Assistant A/B project change is malformed");
            ParameterChange change;
            change.parameterId = child["parameterId"].toString();
            change.reason = child["reason"].toString();
            if (!parseFloat(child["currentNormalized"], change.currentNormalized)
                || !parseFloat(child["proposedNormalized"], change.proposedNormalized))
                return juce::Result::fail("Assistant A/B project change has invalid values");
            proposal.changes.push_back(std::move(change));
        }
        else if (child.hasType(assumptionType))
        {
            if (!hasOnlyProperties(child, assumptionProperties)
                || child.getNumChildren() != 0 || !child.hasProperty("text")
                || !child["text"].isString())
                return juce::Result::fail("Assistant A/B project assumption is malformed");
            proposal.assumptions.push_back(child["text"].toString());
        }
        else
        {
            return juce::Result::fail("Assistant A/B project proposal has an unknown child");
        }
        if (proposal.changes.size() > ParameterProposal::maximumChanges
            || proposal.assumptions.size() > 12)
            return juce::Result::fail("Assistant A/B project proposal is oversized");
    }
    if (const auto validation = validateParameterProposal(proposal); validation.failed())
        return validation;
    snapshot = {*status, *side, std::move(proposal), {}};
    return juce::Result::ok();
}
}
