#pragma once

#include "midi/Composition.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace folkpark::assistant
{
enum class SoundEntryMode : std::uint8_t
{
    describe,
    guided,
    manual,
    count
};

struct SoundAnswers
{
    juce::String musicalRole;
    juce::String timbre;
    juce::String articulation;
    juce::String movement;
    juce::String space;
    float intensity = 0.5f;
    juce::String genreContext;
    juce::String referenceDescription;
};

struct SoundIntent
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String requestId;
    std::uint32_t seed = 0;
    SoundEntryMode entryMode = SoundEntryMode::guided;
    SoundAnswers answers;
};

struct ParameterChange
{
    juce::String parameterId;
    float currentNormalized = 0.0f;
    float proposedNormalized = 0.0f;
    juce::String reason;
};

struct ParameterProposal
{
    static constexpr int currentSchemaVersion = 1;
    static constexpr std::size_t maximumChanges = 73;

    int schemaVersion = currentSchemaVersion;
    juce::String proposalId;
    juce::String requestId;
    std::vector<ParameterChange> changes;
    juce::String explanation;
    std::vector<juce::String> assumptions;
    float confidence = 0.0f;
    bool requiresExplicitAcceptance = true;
};

[[nodiscard]] juce::String stableId(SoundEntryMode value);
[[nodiscard]] juce::Result validateSoundIntent(const SoundIntent& intent);
[[nodiscard]] juce::Result validateParameterProposal(const ParameterProposal& proposal);
}
