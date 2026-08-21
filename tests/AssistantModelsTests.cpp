#include "assistant/AssistantModels.h"

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

    if (failures == 0)
        std::cout << "PASS: M3 guided-sound intent and explicit proposal schema models\n";
    return failures == 0 ? 0 : 1;
}
