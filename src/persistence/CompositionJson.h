#pragma once

#include "midi/Composition.h"

#include <juce_core/juce_core.h>

namespace folkpark::persistence
{
inline constexpr std::int64_t maximumHistoryPayloadBytes = 4 * 1024 * 1024;

struct CompositionJsonResult
{
    juce::Result status = juce::Result::fail("Composition JSON codec did not run");
    juce::String json;
    midi::CompositionBundle bundle;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

struct MusicIntentJsonResult
{
    juce::Result status = juce::Result::fail("MusicIntent JSON codec did not run");
    juce::String json;
    midi::MusicIntent intent;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

[[nodiscard]] MusicIntentJsonResult encodeMusicIntentJson(const midi::MusicIntent& intent);
[[nodiscard]] MusicIntentJsonResult decodeMusicIntentJson(const juce::String& json);
[[nodiscard]] CompositionJsonResult encodeCompositionJson(const midi::CompositionBundle& bundle);
[[nodiscard]] CompositionJsonResult decodeCompositionJson(const juce::String& json);
}
