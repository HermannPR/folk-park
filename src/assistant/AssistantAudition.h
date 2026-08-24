#pragma once

#include "AssistantContracts.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace folkpark::assistant
{
enum class AuditionSide : std::uint8_t
{
    original,
    proposal,
    count
};

struct AssistantAuditionSnapshot
{
    AssistantSessionStatus status = AssistantSessionStatus::idle;
    AuditionSide audibleSide = AuditionSide::original;
    std::optional<ParameterProposal> proposal;
    juce::String message;

    [[nodiscard]] bool active() const noexcept
    {
        return proposal.has_value()
            && (status == AssistantSessionStatus::proposalReady
                || status == AssistantSessionStatus::previewing);
    }
};

class AssistantAuditionSession final
{
public:
    using ApplyValues = std::function<void(std::span<const CurrentParameterValue>)>;

    [[nodiscard]] juce::Result begin(const ParameterProposal& proposal,
                                     std::span<const CurrentParameterValue> currentParameters);
    [[nodiscard]] juce::Result restore(const AssistantAuditionSnapshot& snapshot,
                                       std::span<const CurrentParameterValue> currentParameters);
    [[nodiscard]] juce::Result audition(AuditionSide side,
                                        std::span<const CurrentParameterValue> currentParameters,
                                        const ApplyValues& apply);
    [[nodiscard]] juce::Result accept(std::span<const CurrentParameterValue> currentParameters,
                                      const ApplyValues& apply);
    [[nodiscard]] juce::Result reject(std::span<const CurrentParameterValue> currentParameters,
                                      const ApplyValues& apply);
    void invalidate(const juce::String& reason);
    void reset();

    [[nodiscard]] AssistantAuditionSnapshot snapshot() const { return state; }

private:
    [[nodiscard]] juce::Result validateAudibleValues(
        std::span<const CurrentParameterValue> currentParameters);
    [[nodiscard]] std::vector<CurrentParameterValue> valuesFor(AuditionSide side) const;
    [[nodiscard]] juce::Result applySide(AuditionSide side,
                                         std::span<const CurrentParameterValue> currentParameters,
                                         const ApplyValues& apply,
                                         bool finish);

    AssistantAuditionSnapshot state;
};

[[nodiscard]] juce::String stableId(AuditionSide value);
[[nodiscard]] juce::ValueTree serialiseAssistantAudition(
    const AssistantAuditionSnapshot& snapshot);
[[nodiscard]] juce::Result parseAssistantAudition(
    const juce::ValueTree& tree,
    AssistantAuditionSnapshot& snapshot);
}
