#include "AssistantContracts.h"

#include <array>

namespace folkpark::assistant
{
namespace
{
template <typename Enum, std::size_t Size>
juce::String enumId(Enum value, Enum count, const std::array<const char*, Size>& names)
{
    const auto index = static_cast<std::size_t>(value);
    return value < count && index < names.size() ? juce::String(names[index]) : juce::String{};
}

bool validDisplayText(const juce::String& text, int maximumLength, bool allowEmpty) noexcept
{
    if (text.length() > maximumLength || (!allowEmpty && text.trim().isEmpty()))
        return false;
    for (auto cursor = text.getCharPointer(); !cursor.isEmpty(); ++cursor)
    {
        const auto character = *cursor;
        if (character < 0x20 && character != '\n' && character != '\t')
            return false;
    }
    return true;
}
}

juce::String stableId(AssistantTarget value)
{
    static constexpr std::array names{"composition", "sound"};
    return enumId(value, AssistantTarget::count, names);
}

juce::String stableId(AssistantOrigin value)
{
    static constexpr std::array names{"offline", "mock-provider", "remote-provider"};
    return enumId(value, AssistantOrigin::count, names);
}

juce::String stableId(AssistantSessionStatus value)
{
    static constexpr std::array names{
        "idle", "collecting", "ready", "working", "proposal-ready",
        "previewing", "accepted", "rejected", "cancelled", "failed"};
    return enumId(value, AssistantSessionStatus::count, names);
}

juce::Result validateAssistantRequest(const AssistantRequest& request)
{
    if (request.schemaVersion != AssistantRequest::currentSchemaVersion)
        return juce::Result::fail("Assistant request schema version is unsupported");
    if (!midi::isUuid(request.requestId))
        return juce::Result::fail("Assistant request ID must be a UUID");
    if (stableId(request.target).isEmpty() || stableId(request.origin).isEmpty())
        return juce::Result::fail("Assistant request target or origin is unsupported");
    if (!validDisplayText(request.prompt, AssistantRequest::maximumPromptLength, false))
        return juce::Result::fail("Assistant request prompt is empty or outside its text bound");
    if (request.origin == AssistantOrigin::remoteProvider && !request.producerConsentedToRemote)
        return juce::Result::fail("Remote assistant processing requires explicit producer consent");

    if (request.target == AssistantTarget::composition)
    {
        if (!request.compositionFallback.has_value() || request.soundIntent.has_value())
            return juce::Result::fail("Composition assistant request has the wrong typed context");
        if (request.compositionFallback->requestId != request.requestId)
            return juce::Result::fail("Composition assistant context must match the request ID");
        if (const auto validation = midi::validateMusicIntent(*request.compositionFallback);
            validation.failed())
            return validation;
    }
    else
    {
        if (!request.soundIntent.has_value() || request.compositionFallback.has_value())
            return juce::Result::fail("Sound assistant request has the wrong typed context");
        if (request.soundIntent->requestId != request.requestId)
            return juce::Result::fail("Sound assistant context must match the request ID");
        if (const auto validation = validateSoundIntent(*request.soundIntent); validation.failed())
            return validation;
    }
    return juce::Result::ok();
}

juce::Result validateAssistantResponse(const AssistantResponse& response)
{
    if (response.schemaVersion != AssistantResponse::currentSchemaVersion)
        return juce::Result::fail("Assistant response schema version is unsupported");
    if (!midi::isUuid(response.requestId))
        return juce::Result::fail("Assistant response request ID must be a UUID");
    if (stableId(response.target).isEmpty() || stableId(response.origin).isEmpty())
        return juce::Result::fail("Assistant response target or origin is unsupported");
    if (!validDisplayText(response.summary, AssistantResponse::maximumSummaryLength, false))
        return juce::Result::fail("Assistant response summary is empty or outside its text bound");

    if (response.target == AssistantTarget::composition)
    {
        if (!response.musicIntent.has_value() || response.parameterProposal.has_value())
            return juce::Result::fail("Composition assistant response has the wrong typed result");
        if (response.musicIntent->requestId != response.requestId)
            return juce::Result::fail("Composition result must match the response request ID");
        if (const auto validation = midi::validateMusicIntent(*response.musicIntent);
            validation.failed())
            return validation;
    }
    else
    {
        if (!response.parameterProposal.has_value() || response.musicIntent.has_value())
            return juce::Result::fail("Sound assistant response has the wrong typed result");
        if (response.parameterProposal->requestId != response.requestId)
            return juce::Result::fail("Sound proposal must match the response request ID");
        if (const auto validation = validateParameterProposal(*response.parameterProposal);
            validation.failed())
            return validation;
    }
    return juce::Result::ok();
}

juce::Result validateAssistantExchange(const AssistantRequest& request,
                                       const AssistantResponse& response)
{
    if (const auto validation = validateAssistantRequest(request); validation.failed())
        return validation;
    if (const auto validation = validateAssistantResponse(response); validation.failed())
        return validation;
    if (request.requestId != response.requestId || request.target != response.target
        || request.origin != response.origin)
        return juce::Result::fail("Assistant response does not match the active request");
    return juce::Result::ok();
}
}
