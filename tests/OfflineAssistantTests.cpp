#include "assistant/OfflineAssistant.h"
#include "common/ParameterIds.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

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

folkpark::assistant::AssistantRequest compositionRequest(
    folkpark::assistant::AssistantOrigin origin = folkpark::assistant::AssistantOrigin::offline)
{
    using namespace folkpark;
    assistant::AssistantRequest request;
    request.requestId = midi::deterministicUuid(701, "offline-composition-request");
    request.target = assistant::AssistantTarget::composition;
    request.origin = origin;
    request.prompt = "Create a dark 4 bar D minor progression with a melodic techno bassline at 126 bpm";
    midi::MusicIntent fallback;
    fallback.requestId = request.requestId;
    fallback.seed = 99;
    fallback.lengthBars = 8;
    request.compositionFallback = fallback;
    return request;
}

folkpark::assistant::SoundIntent completeGuidedIntent(const juce::String& requestId)
{
    folkpark::assistant::SoundIntent intent;
    intent.requestId = requestId;
    intent.entryMode = folkpark::assistant::SoundEntryMode::guided;
    intent.seed = 808;
    intent.answers.musicalRole = "wide lead";
    intent.answers.timbre = "bright and glassy";
    intent.answers.articulation = "plucky";
    intent.answers.movement = "moving and wide";
    intent.answers.space = "spacious reverb";
    intent.answers.intensity = 0.8f;
    intent.answers.genreContext = "melodic techno";
    return intent;
}

folkpark::assistant::AssistantRequest soundRequest(
    folkpark::assistant::AssistantOrigin origin = folkpark::assistant::AssistantOrigin::offline)
{
    using namespace folkpark;
    assistant::AssistantRequest request;
    request.requestId = midi::deterministicUuid(702, "offline-sound-request");
    request.target = assistant::AssistantTarget::sound;
    request.origin = origin;
    request.prompt = "Help me build a bright plucky lead with movement and space";
    request.soundIntent = completeGuidedIntent(request.requestId);
    return request;
}

std::vector<folkpark::assistant::CurrentParameterValue> proposalSnapshot()
{
    using namespace folkpark;
    return {
        {parameterIds::filterCutoff, 0.45f},
        {parameterIds::oscillatorAPosition, 0.40f},
        {parameterIds::ampAttack, 0.20f},
        {parameterIds::ampDecay, 0.50f},
        {parameterIds::ampSustain, 0.70f},
        {parameterIds::ampRelease, 0.40f},
        {parameterIds::oscillatorAUnison, 0.14f},
        {parameterIds::oscillatorASpread, 0.25f},
        {parameterIds::oscillatorADetune, 0.08f},
        {parameterIds::chorusBypass, 1.0f},
        {parameterIds::chorusRate, 0.10f},
        {parameterIds::chorusDepth, 0.10f},
        {parameterIds::reverbBypass, 1.0f},
        {parameterIds::reverbRoomSize, 0.30f},
        {parameterIds::reverbMix, 0.10f},
        {parameterIds::distortionBypass, 1.0f},
        {parameterIds::distortionDrive, 0.10f},
        {parameterIds::distortionMix, 0.05f}
    };
}

const folkpark::assistant::ParameterChange* findChange(
    const folkpark::assistant::ParameterProposal& proposal,
    const juce::String& parameterId)
{
    for (const auto& change : proposal.changes)
        if (change.parameterId == parameterId)
            return &change;
    return nullptr;
}
}

int main()
{
    using namespace folkpark;
    assistant::OfflineAssistantEngine engine;

    const auto composition = compositionRequest();
    const auto firstComposition = engine.respond(composition);
    const auto secondComposition = engine.respond(composition);
    expect(firstComposition.status.wasOk() && firstComposition.response.has_value(),
           "Offline composition text must produce a typed candidate");
    expect(secondComposition.status.wasOk() && secondComposition.response.has_value(),
           "The same offline composition request must remain available without a provider");
    if (firstComposition.response && secondComposition.response)
    {
        const auto& first = *firstComposition.response->musicIntent;
        const auto& second = *secondComposition.response->musicIntent;
        expect(first.seed == second.seed, "The same prompt and fallback seed must be deterministic");
        expect(first.key == midi::KeyRoot::d && first.scale == midi::ScaleType::naturalMinor,
               "D minor must be parsed into typed key and scale fields");
        expect(first.lengthBars == 4 && std::abs(first.tempoBpm - 126.0) < 0.001,
               "Word-form bars and numeric BPM must parse into bounded composition fields");
        expect(first.genreProfile == midi::GenreProfile::melodicTechno
                   && first.emotion == midi::Emotion::dark,
               "Genre and emotion words must map to typed context");
        std::set<midi::PartType> parts(first.parts.begin(), first.parts.begin()
                                      + static_cast<std::ptrdiff_t>(first.partCount));
        expect(parts.contains(midi::PartType::chords) && parts.contains(midi::PartType::bass),
               "Progression and bassline requests must select both requested parts");
        expect(firstComposition.response->parameterProposal == std::nullopt,
               "Composition assistance must not mutate sound parameters");
    }

    auto boundedRequest = compositionRequest();
    boundedRequest.prompt = "Create 200 bars at 999 bpm in F# harmonic minor with an arp and melody";
    const auto bounded = engine.respond(boundedRequest);
    expect(bounded.status.wasOk() && bounded.response.has_value(),
           "Recognized out-of-range composition text must normalize instead of escaping bounds");
    if (bounded.response)
    {
        const auto& intent = *bounded.response->musicIntent;
        expect(intent.key == midi::KeyRoot::fSharp
                   && intent.scale == midi::ScaleType::harmonicMinor,
               "Sharp keys and harmonic minor must parse deterministically");
        expect(intent.lengthBars == 64 && std::abs(intent.tempoBpm - 400.0) < 0.001,
               "Composition length and tempo must clamp to the existing domain bounds");
    }

    auto remoteComposition = compositionRequest(assistant::AssistantOrigin::remoteProvider);
    remoteComposition.producerConsentedToRemote = true;
    expect(engine.respond(remoteComposition).status.failed(),
           "The offline engine must never impersonate a remote provider");

    assistant::SoundIntent unanswered;
    unanswered.requestId = midi::deterministicUuid(703, "guided-progress");
    unanswered.entryMode = assistant::SoundEntryMode::guided;
    const auto firstStep = engine.questionsFor(unanswered);
    expect(firstStep.questions.size() == assistant::GuidedProgress::maximumQuestionsPerStep,
           "Guided mode must ask at most two focused questions per step");
    expect(firstStep.questions.size() == 2
               && firstStep.questions[0].topic == assistant::SoundQuestionTopic::musicalRole
               && firstStep.questions[1].topic == assistant::SoundQuestionTopic::timbre,
           "Guided questions must follow a stable producer-friendly order");
    expect(!firstStep.readyForProposal && firstStep.completion == 0.0f,
           "An unanswered walkthrough must not create a proposal");

    const auto completeGuided = completeGuidedIntent(unanswered.requestId);
    const auto completeProgress = engine.questionsFor(completeGuided);
    expect(completeProgress.readyForProposal && completeProgress.questions.empty()
               && std::abs(completeProgress.completion - 1.0f) < 0.001f,
           "Seven required guided answers must make the intent proposal-ready");

    const auto snapshot = proposalSnapshot();
    expect(assistant::validateCurrentParameterValues(snapshot).wasOk(),
           "A unique finite host-parameter snapshot must validate");
    const auto firstSound = engine.respond(soundRequest(), snapshot);
    const auto secondSound = engine.respond(soundRequest(), snapshot);
    expect(firstSound.status.wasOk() && firstSound.response.has_value(),
           "A complete guided sound intent must produce an explained proposal");
    expect(secondSound.status.wasOk() && secondSound.response.has_value(),
           "Offline sound proposals must be repeatable without a provider account");
    if (firstSound.response && secondSound.response)
    {
        const auto& proposal = *firstSound.response->parameterProposal;
        const auto& repeated = *secondSound.response->parameterProposal;
        expect(proposal.proposalId == repeated.proposalId
                   && proposal.changes.size() == repeated.changes.size(),
               "The same sound answers and seed must create the same proposal identity");
        expect(assistant::validateParameterProposal(proposal).wasOk(),
               "Offline proposals must pass the same catalog validation as provider output");
        expect(proposal.requiresExplicitAcceptance,
               "Offline proposals must preserve the explicit acceptance boundary");
        expect(findChange(proposal, parameterIds::filterCutoff) != nullptr
                   && findChange(proposal, parameterIds::ampAttack) != nullptr
                   && findChange(proposal, parameterIds::oscillatorAUnison) != nullptr
                   && findChange(proposal, parameterIds::reverbMix) != nullptr,
               "Bright, plucky, wide, and spacious answers must affect relevant host controls");
        for (const auto& change : proposal.changes)
        {
            expect(assistant::isKnownParameterId(change.parameterId),
                   "Every proposed change must use a real stable host parameter ID");
            const auto source = std::find_if(snapshot.begin(), snapshot.end(), [&](const auto& value)
            {
                return value.parameterId == change.parameterId;
            });
            expect(source != snapshot.end()
                       && std::abs(source->normalized - change.currentNormalized) < 1.0e-6f,
                   "Every A value must equal the captured current host value");
        }
    }

    auto reorderedSnapshot = snapshot;
    std::reverse(reorderedSnapshot.begin(), reorderedSnapshot.end());
    const auto reorderedSound = engine.respond(soundRequest(), reorderedSnapshot);
    expect(reorderedSound.status.wasOk() && reorderedSound.response.has_value()
               && firstSound.response.has_value()
               && reorderedSound.response->parameterProposal->proposalId
                   == firstSound.response->parameterProposal->proposalId,
           "Proposal identity must not depend on host snapshot iteration order");
    auto changedSnapshot = snapshot;
    changedSnapshot.front().normalized = 0.46f;
    const auto changedSound = engine.respond(soundRequest(), changedSnapshot);
    expect(changedSound.status.wasOk() && changedSound.response.has_value()
               && firstSound.response.has_value()
               && changedSound.response->parameterProposal->proposalId
                   != firstSound.response->parameterProposal->proposalId,
           "A different current synth state must produce a different proposal identity");

    auto describe = soundRequest();
    describe.requestId = midi::deterministicUuid(704, "describe-sound-request");
    describe.prompt = "A warm sustained atmospheric pad";
    describe.soundIntent = assistant::SoundIntent{};
    describe.soundIntent->requestId = describe.requestId;
    describe.soundIntent->entryMode = assistant::SoundEntryMode::describe;
    describe.soundIntent->answers.referenceDescription = describe.prompt;
    const auto described = engine.respond(describe, snapshot);
    expect(described.status.wasOk() && described.response.has_value(),
           "Describe mode must produce a bounded proposal and state its assumptions");
    if (described.response)
        expect(!described.response->parameterProposal->assumptions.empty(),
               "A one-line sound description must expose inferred fields as assumptions");

    auto incomplete = soundRequest();
    incomplete.soundIntent->answers = {};
    incomplete.soundIntent->answers.musicalRole = "bass";
    const auto needsAnswers = engine.respond(incomplete, snapshot);
    expect(needsAnswers.status.failed() && needsAnswers.retryable && !needsAnswers.response,
           "Incomplete guided input must ask for more detail instead of inventing a proposal");

    auto manual = soundRequest();
    manual.soundIntent->entryMode = assistant::SoundEntryMode::manual;
    manual.soundIntent->answers = {};
    expect(engine.respond(manual, snapshot).status.failed(),
           "Manual mode must not create an assistant proposal");

    auto invalidSnapshot = snapshot;
    invalidSnapshot.push_back(invalidSnapshot.front());
    expect(assistant::validateCurrentParameterValues(invalidSnapshot).failed(),
           "Duplicate current host parameter IDs must be rejected");
    invalidSnapshot = snapshot;
    invalidSnapshot.front().parameterId = "unknownParameter";
    expect(assistant::validateCurrentParameterValues(invalidSnapshot).failed(),
           "Unknown current host parameter IDs must be rejected");
    invalidSnapshot = snapshot;
    invalidSnapshot.front().normalized = std::numeric_limits<float>::quiet_NaN();
    expect(assistant::validateCurrentParameterValues(invalidSnapshot).failed(),
           "Non-finite current host values must be rejected");

    assistant::MockAssistantProvider mock(engine);
    auto mockRequest = soundRequest(assistant::AssistantOrigin::mockProvider);
    mock.setCurrentParameters(snapshot);
    int callbackCount = 0;
    assistant::AssistantProviderResult callbackResult;
    expect(mock.submit(mockRequest, [&](assistant::AssistantProviderResult result)
    {
        ++callbackCount;
        callbackResult = std::move(result);
    }).wasOk(), "The mock provider must queue one valid typed request");
    expect(mock.hasPendingRequest(), "The mock request must remain pending until controlled completion");
    expect(mock.submit(mockRequest, [](auto) {}).failed(),
           "The mock provider must reject a second request while one is pending");
    expect(mock.completePending().wasOk() && callbackCount == 1
               && callbackResult.status.wasOk() && callbackResult.response.has_value(),
           "Controlled mock completion must invoke the callback exactly once");
    expect(mock.completePending().failed() && callbackCount == 1,
           "A completed mock request must not complete again");

    callbackCount = 0;
    expect(mock.submit(mockRequest, [&](assistant::AssistantProviderResult result)
    {
        ++callbackCount;
        callbackResult = std::move(result);
    }).wasOk(), "The mock provider must accept a new request after completion");
    mock.cancel(mockRequest.requestId);
    expect(callbackCount == 1 && callbackResult.status.failed() && callbackResult.retryable
               && !mock.hasPendingRequest(),
           "Cancellation must complete once, be retryable, and clear pending state");
    expect(mock.completePending().failed() && callbackCount == 1,
           "A cancelled mock request must never complete later");

    auto wrongOrigin = soundRequest();
    expect(mock.submit(wrongOrigin, [](auto) {}).failed(),
           "The mock provider must reject requests from a different origin");

    if (failures == 0)
        std::cout << "PASS: deterministic offline composition, guided sound, and mock provider workflows\n";
    return failures == 0 ? 0 : 1;
}
