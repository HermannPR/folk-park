#pragma once

#include "AssistantContracts.h"

#include <juce_core/juce_core.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace folkpark::assistant
{
enum class SoundQuestionTopic : std::uint8_t
{
    musicalRole,
    timbre,
    articulation,
    movement,
    space,
    intensity,
    genreContext,
    referenceDescription,
    count
};

struct GuidedQuestion
{
    SoundQuestionTopic topic = SoundQuestionTopic::musicalRole;
    juce::String id;
    juce::String prompt;
    juce::String purpose;
    bool required = true;
};

struct GuidedProgress
{
    static constexpr std::size_t maximumQuestionsPerStep = 2;

    float completion = 0.0f;
    bool readyForProposal = false;
    std::vector<GuidedQuestion> questions;
};

[[nodiscard]] juce::String stableId(SoundQuestionTopic value);

class OfflineAssistantEngine final
{
public:
    [[nodiscard]] GuidedProgress questionsFor(const SoundIntent& intent) const;
    [[nodiscard]] AssistantProviderResult respond(
        const AssistantRequest& request,
        std::span<const CurrentParameterValue> currentParameters = {}) const;

private:
    [[nodiscard]] AssistantProviderResult respondToComposition(
        const AssistantRequest& request) const;
    [[nodiscard]] AssistantProviderResult respondToSound(
        const AssistantRequest& request,
        std::span<const CurrentParameterValue> currentParameters) const;
};

class MockAssistantProvider final : public AssistantProvider
{
public:
    explicit MockAssistantProvider(const OfflineAssistantEngine& engineToUse);

    [[nodiscard]] juce::String providerId() const override;
    [[nodiscard]] AssistantOrigin origin() const noexcept override;
    juce::Result submit(const AssistantRequest& request, Completion completion) override;
    void cancel(const juce::String& requestId) override;

    void setCurrentParameters(std::vector<CurrentParameterValue> values);
    [[nodiscard]] bool hasPendingRequest() const noexcept;
    [[nodiscard]] juce::Result completePending();

private:
    struct Pending
    {
        AssistantRequest request;
        Completion completion;
    };

    const OfflineAssistantEngine& engine;
    std::vector<CurrentParameterValue> currentParameters;
    std::optional<Pending> pending;
};
}
