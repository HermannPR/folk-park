#pragma once

#include "AssistantModels.h"
#include "midi/Composition.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <functional>
#include <optional>

namespace folkpark::assistant
{
enum class AssistantTarget : std::uint8_t
{
    composition,
    sound,
    count
};

enum class AssistantOrigin : std::uint8_t
{
    offline,
    mockProvider,
    remoteProvider,
    count
};

enum class AssistantSessionStatus : std::uint8_t
{
    idle,
    collecting,
    ready,
    working,
    proposalReady,
    previewing,
    accepted,
    rejected,
    cancelled,
    failed,
    count
};

struct AssistantRequest
{
    static constexpr int currentSchemaVersion = 1;
    static constexpr int maximumPromptLength = 1024;

    int schemaVersion = currentSchemaVersion;
    juce::String requestId;
    AssistantTarget target = AssistantTarget::sound;
    AssistantOrigin origin = AssistantOrigin::offline;
    juce::String prompt;
    bool producerConsentedToRemote = false;
    std::optional<midi::MusicIntent> compositionFallback;
    std::optional<SoundIntent> soundIntent;
};

struct AssistantResponse
{
    static constexpr int currentSchemaVersion = 1;
    static constexpr int maximumSummaryLength = 1024;

    int schemaVersion = currentSchemaVersion;
    juce::String requestId;
    AssistantTarget target = AssistantTarget::sound;
    AssistantOrigin origin = AssistantOrigin::offline;
    std::optional<midi::MusicIntent> musicIntent;
    std::optional<ParameterProposal> parameterProposal;
    juce::String summary;
};

struct AssistantProviderResult
{
    juce::Result status = juce::Result::fail("Assistant provider did not run");
    std::optional<AssistantResponse> response;
    bool retryable = false;
};

class AssistantProvider
{
public:
    using Completion = std::function<void(AssistantProviderResult)>;

    virtual ~AssistantProvider() = default;
    [[nodiscard]] virtual juce::String providerId() const = 0;
    [[nodiscard]] virtual AssistantOrigin origin() const noexcept = 0;

    // submit and cancel are non-audio-thread APIs. Implementations may invoke completion
    // asynchronously, but must invoke it at most once for an accepted request.
    virtual juce::Result submit(const AssistantRequest& request, Completion completion) = 0;
    virtual void cancel(const juce::String& requestId) = 0;
};

[[nodiscard]] juce::String stableId(AssistantTarget value);
[[nodiscard]] juce::String stableId(AssistantOrigin value);
[[nodiscard]] juce::String stableId(AssistantSessionStatus value);
[[nodiscard]] juce::Result validateAssistantRequest(const AssistantRequest& request);
[[nodiscard]] juce::Result validateAssistantResponse(const AssistantResponse& response);
[[nodiscard]] juce::Result validateAssistantExchange(const AssistantRequest& request,
                                                     const AssistantResponse& response);
}
