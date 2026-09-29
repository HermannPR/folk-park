#include "assistant/AssistantModels.h"
#include "assistant/AssistantContracts.h"
#include "common/ParameterIds.h"

#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main()
{
    using namespace folkpark;
    assistant::SoundIntent sound;
    sound.requestId = midi::deterministicUuid(1, "sound-intent");
    sound.answers.musicalRole = "wide melodic lead";
    sound.answers.timbre = "bright but smooth";
    expect(assistant::validateSoundIntent(sound).wasOk(),
           "Bounded guided SoundIntent must validate");
    sound.answers.intensity = 2.0f;
    expect(assistant::validateSoundIntent(sound).failed(),
           "Unbounded guided intensity must be rejected");

    assistant::ParameterProposal proposal;
    proposal.proposalId = midi::deterministicUuid(2, "parameter-proposal");
    proposal.requestId = sound.requestId;
    proposal.changes.push_back({"filterCutoff", 0.4f, 0.7f, "Adds brightness"});
    proposal.explanation = "A reviewable brighter lead proposal";
    proposal.confidence = 0.75f;
    expect(assistant::validateParameterProposal(proposal).wasOk(),
           "Bounded explained proposal with explicit acceptance must validate");
    proposal.requiresExplicitAcceptance = false;
    expect(assistant::validateParameterProposal(proposal).failed(),
           "No assistant proposal may bypass explicit producer acceptance");
    proposal.requiresExplicitAcceptance = true;
    proposal.changes.push_back(proposal.changes.front());
    expect(assistant::validateParameterProposal(proposal).failed(),
           "Duplicate proposal parameter IDs must be rejected");

    proposal.changes.resize(1);
    proposal.changes.front().parameterId = "notARealHostParameter";
    expect(assistant::validateParameterProposal(proposal).failed(),
           "Unknown proposal parameter IDs must be rejected against the C++ catalog");
    proposal.changes.front().parameterId = parameterIds::distortionDrive;
    expect(assistant::validateParameterProposal(proposal).wasOk(),
           "Schema v2 must admit the complete 102-parameter catalog including effects");

    proposal.schemaVersion = 1;
    expect(assistant::validateParameterProposal(proposal).failed(),
           "Schema v1 must remain frozen to the pre-effects 73-parameter catalog");
    proposal.changes.front().parameterId = parameterIds::filterCutoff;
    expect(assistant::validateParameterProposal(proposal).wasOk(),
           "Oldest-supported schema v1 proposals must remain valid");

    proposal.schemaVersion = assistant::ParameterProposal::currentSchemaVersion;
    proposal.changes.front().reason = "  ";
    expect(assistant::validateParameterProposal(proposal).failed(),
           "Current proposals must explain every parameter change");

    proposal.changes.front().reason = "Keeps the cutoff aligned with the requested timbre";
    assistant::AssistantRequest request;
    request.requestId = sound.requestId;
    request.prompt = "Make this lead brighter without making it harsh";
    request.soundIntent = sound;
    sound.answers.intensity = 0.7f;
    request.soundIntent = sound;
    expect(assistant::validateAssistantRequest(request).wasOk(),
           "Bounded offline sound requests must validate");
    request.prompt = juce::String::repeatedString("x", assistant::AssistantRequest::maximumPromptLength + 1);
    expect(assistant::validateAssistantRequest(request).failed(),
           "Oversized assistant prompts must be rejected before dispatch");
    request.prompt = "unsafe" + juce::String::charToString(1);
    expect(assistant::validateAssistantRequest(request).failed(),
           "Assistant prompts containing disallowed controls must be rejected");
    request.prompt = "Make this lead brighter without making it harsh";
    request.origin = assistant::AssistantOrigin::remoteProvider;
    expect(assistant::validateAssistantRequest(request).failed(),
           "Remote processing must require explicit producer consent");
    request.producerConsentedToRemote = true;
    expect(assistant::validateAssistantRequest(request).wasOk(),
           "A remote request with explicit consent and typed context must validate");

    assistant::AssistantResponse response;
    response.requestId = request.requestId;
    response.target = assistant::AssistantTarget::sound;
    response.origin = assistant::AssistantOrigin::remoteProvider;
    response.parameterProposal = proposal;
    response.summary = "A reviewable brighter lead proposal";
    expect(assistant::validateAssistantResponse(response).wasOk(),
           "A typed catalog-validated provider response must validate");
    expect(assistant::validateAssistantExchange(request, response).wasOk(),
           "Matching request and response variants must validate as one exchange");
    response.origin = assistant::AssistantOrigin::offline;
    expect(assistant::validateAssistantExchange(request, response).failed(),
           "Responses from a different processing origin must be rejected");
    response.origin = assistant::AssistantOrigin::remoteProvider;
    response.parameterProposal->requestId = midi::deterministicUuid(90, "stale-response");
    expect(assistant::validateAssistantResponse(response).failed(),
           "Stale provider responses must not cross request boundaries");

    assistant::AssistantRequest compositionRequest;
    compositionRequest.requestId = midi::deterministicUuid(4, "composition-request");
    compositionRequest.target = assistant::AssistantTarget::composition;
    compositionRequest.prompt = "Create a dark four bar D minor progression";
    midi::MusicIntent compositionFallback;
    compositionFallback.requestId = compositionRequest.requestId;
    compositionRequest.compositionFallback = compositionFallback;
    expect(assistant::validateAssistantRequest(compositionRequest).wasOk(),
           "Composition text requests must carry a valid deterministic fallback intent");
    compositionRequest.soundIntent = sound;
    expect(assistant::validateAssistantRequest(compositionRequest).failed(),
           "Assistant request variants must not mix sound and composition context");

    if (failures == 0)
        std::cout << "PASS: M7 assistant contracts, catalog versions, consent, and typed exchanges\n";
    return failures == 0 ? 0 : 1;
}
