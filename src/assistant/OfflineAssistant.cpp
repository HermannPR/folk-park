#include "OfflineAssistant.h"

#include "common/ParameterIds.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <set>

namespace folkpark::assistant
{
namespace
{
constexpr std::size_t requiredGuidedAnswerCount = 7;

juce::String normalisedText(const SoundAnswers& answers)
{
    return (answers.musicalRole + " " + answers.timbre + " " + answers.articulation + " "
            + answers.movement + " " + answers.space + " " + answers.genreContext + " "
            + answers.referenceDescription).toLowerCase().trim();
}

bool hasText(const juce::String& value) noexcept
{
    return !value.trim().isEmpty();
}

bool containsAny(const juce::String& text, std::initializer_list<const char*> phrases)
{
    for (const auto* phrase : phrases)
        if (text.contains(phrase))
            return true;
    return false;
}

std::uint32_t fnv1a(const juce::String& text, std::uint32_t seed) noexcept
{
    auto hash = seed ^ 2166136261u;
    const auto utf8 = text.toRawUTF8();
    for (const auto* cursor = utf8; *cursor != 0; ++cursor)
    {
        hash ^= static_cast<std::uint8_t>(*cursor);
        hash *= 16777619u;
    }
    return hash;
}

std::uint32_t proposalSeed(const juce::String& text,
                           std::uint32_t fallbackSeed,
                           std::span<const CurrentParameterValue> currentParameters)
{
    std::vector<const CurrentParameterValue*> ordered;
    ordered.reserve(currentParameters.size());
    for (const auto& value : currentParameters)
        ordered.push_back(&value);
    std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right)
    {
        return left->parameterId < right->parameterId;
    });

    auto seed = fnv1a(text, fallbackSeed);
    for (const auto* value : ordered)
    {
        seed = fnv1a(value->parameterId, seed);
        seed ^= std::bit_cast<std::uint32_t>(value->normalized);
        seed *= 16777619u;
    }
    return seed;
}

juce::StringArray promptTokens(const juce::String& prompt)
{
    juce::StringArray tokens;
    tokens.addTokens(prompt.toLowerCase(), " \t\r\n,.;:!?()[]{}-/", "\"'");
    tokens.removeEmptyStrings(true);
    return tokens;
}

std::optional<midi::KeyRoot> keyForToken(const juce::String& token)
{
    const auto value = token.toLowerCase();
    if (value == "c") return midi::KeyRoot::c;
    if (value == "c#" || value == "db") return midi::KeyRoot::cSharp;
    if (value == "d") return midi::KeyRoot::d;
    if (value == "d#" || value == "eb") return midi::KeyRoot::dSharp;
    if (value == "e" || value == "fb") return midi::KeyRoot::e;
    if (value == "f" || value == "e#") return midi::KeyRoot::f;
    if (value == "f#" || value == "gb") return midi::KeyRoot::fSharp;
    if (value == "g") return midi::KeyRoot::g;
    if (value == "g#" || value == "ab") return midi::KeyRoot::gSharp;
    if (value == "a") return midi::KeyRoot::a;
    if (value == "a#" || value == "bb") return midi::KeyRoot::aSharp;
    if (value == "b" || value == "cb") return midi::KeyRoot::b;
    return std::nullopt;
}

bool isScaleToken(const juce::String& token)
{
    return token == "major" || token == "minor" || token == "dorian"
        || token == "mixolydian" || token == "pentatonic" || token == "harmonic"
        || token == "natural";
}

std::optional<midi::KeyRoot> parsePromptKey(const juce::StringArray& tokens)
{
    for (int index = 0; index < tokens.size(); ++index)
    {
        auto candidateIndex = index;
        if (tokens[index] == "key")
        {
            candidateIndex = index + 1;
            if (candidateIndex < tokens.size() && tokens[candidateIndex] == "of")
                ++candidateIndex;
            if (candidateIndex < tokens.size())
                if (const auto root = keyForToken(tokens[candidateIndex]))
                    return root;
            continue;
        }
        if (const auto root = keyForToken(tokens[candidateIndex]))
            if (candidateIndex + 1 < tokens.size() && isScaleToken(tokens[candidateIndex + 1]))
                return root;
    }
    return std::nullopt;
}

std::optional<int> numberBefore(const juce::StringArray& tokens,
                                std::initializer_list<const char*> suffixes)
{
    const auto parseNumber = [](const juce::String& token) -> std::optional<int>
    {
        if (token.containsOnly("0123456789"))
            return token.getIntValue();
        static constexpr std::array words{
            "one", "two", "three", "four", "five", "six", "seven", "eight",
            "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen"};
        for (std::size_t index = 0; index < words.size(); ++index)
            if (token == words[index])
                return static_cast<int>(index + 1);
        return std::nullopt;
    };
    for (int index = 1; index < tokens.size(); ++index)
    {
        auto suffixMatches = false;
        for (const auto* suffix : suffixes)
            suffixMatches = suffixMatches || tokens[index] == suffix;
        if (suffixMatches)
            if (const auto number = parseNumber(tokens[index - 1]))
                return number;
    }
    return std::nullopt;
}

void applyRecognisedParts(const juce::String& prompt, midi::MusicIntent& intent)
{
    std::array<midi::PartType, 4> parts{};
    std::size_t count = 0;
    const auto add = [&parts, &count](midi::PartType part)
    {
        if (count < parts.size())
            parts[count++] = part;
    };
    if (containsAny(prompt, {"chord", "progression", "harmony"})) add(midi::PartType::chords);
    if (containsAny(prompt, {"melody", "melodic", "lead", "theme"})) add(midi::PartType::melody);
    if (containsAny(prompt, {"bass", "bassline"})) add(midi::PartType::bass);
    if (containsAny(prompt, {"arp", "arpeggio", "arpeggiated"})) add(midi::PartType::arp);
    if (count > 0)
    {
        intent.parts = parts;
        intent.partCount = count;
    }
}

void applyRecognisedContext(const juce::String& prompt, midi::MusicIntent& intent)
{
    if (prompt.contains("melodic techno")) intent.genreProfile = midi::GenreProfile::melodicTechno;
    else if (prompt.contains("synthwave")) intent.genreProfile = midi::GenreProfile::synthwave;
    else if (prompt.contains("ambient")) intent.genreProfile = midi::GenreProfile::ambient;
    else if (prompt.contains("cinematic")) intent.genreProfile = midi::GenreProfile::cinematic;
    else if (prompt.contains("house")) intent.genreProfile = midi::GenreProfile::house;

    if (prompt.contains("dark")) intent.emotion = midi::Emotion::dark;
    else if (prompt.contains("bright")) intent.emotion = midi::Emotion::bright;
    else if (prompt.contains("tense")) intent.emotion = midi::Emotion::tense;
    else if (prompt.contains("hopeful")) intent.emotion = midi::Emotion::hopeful;
    else if (containsAny(prompt, {"dreamy", "dream"})) intent.emotion = midi::Emotion::dreamy;

    if (containsAny(prompt, {"sparse", "minimal"})) intent.density = 0.28f;
    if (containsAny(prompt, {"dense", "busy", "full"})) intent.density = 0.78f;
    if (containsAny(prompt, {"simple rhythm", "straight"})) intent.rhythmComplexity = 0.25f;
    if (containsAny(prompt, {"complex rhythm", "syncopated"})) intent.rhythmComplexity = 0.75f;
    if (containsAny(prompt, {"repetitive", "hypnotic"})) intent.repetition = 0.82f;
    if (containsAny(prompt, {"varied", "evolving"})) intent.variation = 0.72f;
}

void applyRecognisedScale(const juce::String& prompt, midi::MusicIntent& intent)
{
    if (prompt.contains("harmonic minor")) intent.scale = midi::ScaleType::harmonicMinor;
    else if (prompt.contains("natural minor")) intent.scale = midi::ScaleType::naturalMinor;
    else if (prompt.contains("minor pentatonic")) intent.scale = midi::ScaleType::pentatonicMinor;
    else if (prompt.contains("major pentatonic")) intent.scale = midi::ScaleType::pentatonicMajor;
    else if (prompt.contains("dorian")) intent.scale = midi::ScaleType::dorian;
    else if (prompt.contains("mixolydian")) intent.scale = midi::ScaleType::mixolydian;
    else if (prompt.contains("minor")) intent.scale = midi::ScaleType::naturalMinor;
    else if (prompt.contains("major")) intent.scale = midi::ScaleType::major;
}

std::optional<float> currentValue(std::span<const CurrentParameterValue> current,
                                  const juce::String& parameterId)
{
    for (const auto& value : current)
        if (value.parameterId == parameterId)
            return value.normalized;
    return std::nullopt;
}

void addChange(ParameterProposal& proposal,
               std::span<const CurrentParameterValue> current,
               const char* parameterId,
               float proposed,
               const juce::String& reason)
{
    const auto existing = currentValue(current, parameterId);
    const auto bounded = juce::jlimit(0.0f, 1.0f, proposed);
    if (!existing.has_value() || std::abs(*existing - bounded) <= 1.0e-6f)
        return;
    for (const auto& change : proposal.changes)
        if (change.parameterId == parameterId)
            return;
    proposal.changes.push_back({parameterId, *existing, bounded, reason});
}

GuidedQuestion question(SoundQuestionTopic topic)
{
    switch (topic)
    {
        case SoundQuestionTopic::musicalRole:
            return {topic, stableId(topic), "What role should this sound play?",
                    "Role sets register, weight, and envelope priorities", true};
        case SoundQuestionTopic::timbre:
            return {topic, stableId(topic), "How should the tone feel: dark, warm, bright, glassy, or something else?",
                    "Timbre guides oscillator balance and filtering", true};
        case SoundQuestionTopic::articulation:
            return {topic, stableId(topic), "Should notes be plucky, sustained, soft, sharp, or swelling?",
                    "Articulation controls the amplitude shape", true};
        case SoundQuestionTopic::movement:
            return {topic, stableId(topic), "Should it stay stable or move, pulse, wobble, or widen?",
                    "Movement guides modulation and stereo treatment", true};
        case SoundQuestionTopic::space:
            return {topic, stableId(topic), "Should it be dry and close or spacious and atmospheric?",
                    "Space guides delay, reverb, and width", true};
        case SoundQuestionTopic::intensity:
            return {topic, stableId(topic), "How intense should it be from 0 to 100 percent?",
                    "Intensity bounds brightness, drive, and density", true};
        case SoundQuestionTopic::genreContext:
            return {topic, stableId(topic), "What genre or production context should it fit?",
                    "Context helps balance familiar production priorities without copying a preset", true};
        case SoundQuestionTopic::referenceDescription:
            return {topic, stableId(topic), "Any optional reference description or extra detail?",
                    "References are treated as descriptive intent only", false};
        case SoundQuestionTopic::count: break;
    }
    return {};
}

bool answerPresent(const SoundAnswers& answers, SoundQuestionTopic topic)
{
    switch (topic)
    {
        case SoundQuestionTopic::musicalRole: return hasText(answers.musicalRole);
        case SoundQuestionTopic::timbre: return hasText(answers.timbre);
        case SoundQuestionTopic::articulation: return hasText(answers.articulation);
        case SoundQuestionTopic::movement: return hasText(answers.movement);
        case SoundQuestionTopic::space: return hasText(answers.space);
        case SoundQuestionTopic::intensity: return answers.intensity.has_value();
        case SoundQuestionTopic::genreContext: return hasText(answers.genreContext);
        case SoundQuestionTopic::referenceDescription: return hasText(answers.referenceDescription);
        case SoundQuestionTopic::count: break;
    }
    return false;
}
}

juce::String stableId(SoundQuestionTopic value)
{
    static constexpr std::array names{
        "musical-role", "timbre", "articulation", "movement", "space",
        "intensity", "genre-context", "reference-description"};
    const auto index = static_cast<std::size_t>(value);
    return index < names.size() ? juce::String(names[index]) : juce::String{};
}

juce::Result validateCurrentParameterValues(std::span<const CurrentParameterValue> values)
{
    if (values.empty() || values.size() > ParameterProposal::maximumChanges)
        return juce::Result::fail("Assistant current-parameter snapshot is empty or oversized");
    std::set<juce::String> seen;
    for (const auto& value : values)
    {
        if (!isKnownParameterId(value.parameterId) || !std::isfinite(value.normalized)
            || value.normalized < 0.0f || value.normalized > 1.0f)
            return juce::Result::fail("Assistant current-parameter snapshot contains an invalid value");
        if (!seen.insert(value.parameterId).second)
            return juce::Result::fail("Assistant current-parameter snapshot contains a duplicate ID");
    }
    return juce::Result::ok();
}

GuidedProgress OfflineAssistantEngine::questionsFor(const SoundIntent& intent) const
{
    GuidedProgress progress;
    if (intent.entryMode == SoundEntryMode::manual)
    {
        progress.completion = 1.0f;
        return progress;
    }
    if (intent.entryMode == SoundEntryMode::describe
        && hasText(intent.answers.referenceDescription))
    {
        progress.completion = 1.0f;
        progress.readyForProposal = true;
        return progress;
    }

    static constexpr std::array requiredTopics{
        SoundQuestionTopic::musicalRole, SoundQuestionTopic::timbre,
        SoundQuestionTopic::articulation, SoundQuestionTopic::movement,
        SoundQuestionTopic::space, SoundQuestionTopic::intensity,
        SoundQuestionTopic::genreContext};
    std::size_t answered = 0;
    for (const auto topic : requiredTopics)
    {
        if (answerPresent(intent.answers, topic))
        {
            ++answered;
            continue;
        }
        if (progress.questions.size() < GuidedProgress::maximumQuestionsPerStep)
            progress.questions.push_back(question(topic));
    }
    progress.completion = static_cast<float>(answered)
        / static_cast<float>(requiredGuidedAnswerCount);
    progress.readyForProposal = answered == requiredGuidedAnswerCount;
    return progress;
}

AssistantProviderResult OfflineAssistantEngine::respond(
    const AssistantRequest& request,
    std::span<const CurrentParameterValue> currentParameters) const
{
    if (const auto validation = validateAssistantRequest(request); validation.failed())
        return {validation, std::nullopt, false};
    if (request.origin == AssistantOrigin::remoteProvider)
        return {juce::Result::fail("Offline engine cannot execute a remote request"), std::nullopt, false};
    return request.target == AssistantTarget::composition
        ? respondToComposition(request) : respondToSound(request, currentParameters);
}

AssistantProviderResult OfflineAssistantEngine::respondToComposition(
    const AssistantRequest& request) const
{
    auto intent = *request.compositionFallback;
    const auto prompt = request.prompt.toLowerCase();
    const auto tokens = promptTokens(prompt);
    if (const auto key = parsePromptKey(tokens)) intent.key = *key;
    applyRecognisedScale(prompt, intent);
    applyRecognisedParts(prompt, intent);
    applyRecognisedContext(prompt, intent);
    if (const auto bars = numberBefore(tokens, {"bar", "bars"})) intent.lengthBars = *bars;
    if (const auto bpm = numberBefore(tokens, {"bpm"})) intent.tempoBpm = *bpm;
    intent.seed = fnv1a(prompt, intent.seed);
    if (const auto validation = midi::normaliseAndValidate(intent); validation.failed())
        return {validation, std::nullopt, false};

    AssistantResponse response;
    response.requestId = request.requestId;
    response.target = request.target;
    response.origin = request.origin;
    response.musicIntent = intent;
    response.summary = "Offline Jarvis prepared a deterministic composition intent for review";
    if (const auto validation = validateAssistantExchange(request, response); validation.failed())
        return {validation, std::nullopt, false};
    return {juce::Result::ok(), std::move(response), false};
}

AssistantProviderResult OfflineAssistantEngine::respondToSound(
    const AssistantRequest& request,
    std::span<const CurrentParameterValue> currentParameters) const
{
    if (const auto validation = validateCurrentParameterValues(currentParameters);
        validation.failed())
        return {validation, std::nullopt, false};
    auto intent = *request.soundIntent;
    if (intent.entryMode == SoundEntryMode::describe
        && intent.answers.referenceDescription.trim().isEmpty())
        intent.answers.referenceDescription = request.prompt;
    if (intent.entryMode == SoundEntryMode::manual)
        return {juce::Result::fail("Manual mode does not create assistant proposals"), std::nullopt, false};
    const auto progress = questionsFor(intent);
    if (!progress.readyForProposal)
        return {juce::Result::fail("Guided sound intent needs another focused answer"), std::nullopt, true};

    const auto text = normalisedText(intent.answers);
    ParameterProposal proposal;
    proposal.proposalId = midi::deterministicUuid(
        proposalSeed(text, intent.seed, currentParameters), "offline-sound-proposal");
    proposal.requestId = request.requestId;

    const auto intensity = intent.answers.intensity.value_or(0.5f);
    if (containsAny(text, {"dark", "warm", "mellow"}))
    {
        addChange(proposal, currentParameters, parameterIds::filterCutoff, 0.34f,
                  "Reduces upper harmonics for the requested darker tone");
        addChange(proposal, currentParameters, parameterIds::oscillatorAPosition, 0.22f,
                  "Moves oscillator A toward a smoother frame region");
    }
    if (containsAny(text, {"bright", "glassy", "shimmer", "air"}))
    {
        addChange(proposal, currentParameters, parameterIds::filterCutoff, 0.76f,
                  "Opens the filter for the requested brightness");
        addChange(proposal, currentParameters, parameterIds::oscillatorAPosition, 0.68f,
                  "Moves oscillator A toward a more harmonically detailed frame");
    }
    if (containsAny(text, {"pluck", "plucky", "short", "percussive"}))
    {
        addChange(proposal, currentParameters, parameterIds::ampAttack, 0.03f,
                  "Keeps the transient immediate");
        addChange(proposal, currentParameters, parameterIds::ampDecay, 0.20f,
                  "Creates a compact decay for a plucked articulation");
        addChange(proposal, currentParameters, parameterIds::ampSustain, 0.16f,
                  "Reduces the held body after the transient");
        addChange(proposal, currentParameters, parameterIds::ampRelease, 0.15f,
                  "Prevents the pluck tail from becoming smeared");
    }
    if (containsAny(text, {"pad", "sustain", "sustained", "swelling", "soft attack"}))
    {
        addChange(proposal, currentParameters, parameterIds::ampAttack, 0.38f,
                  "Softens the onset for the requested sustained role");
        addChange(proposal, currentParameters, parameterIds::ampSustain, 0.82f,
                  "Maintains the body while notes are held");
        addChange(proposal, currentParameters, parameterIds::ampRelease, 0.52f,
                  "Adds a controlled trailing envelope");
    }
    if (containsAny(text, {"bass", "sub", "low end"}))
    {
        addChange(proposal, currentParameters, parameterIds::subLevel, 0.62f,
                  "Adds a bounded fundamental layer for the bass role");
        addChange(proposal, currentParameters, parameterIds::oscillatorACoarse, 0.333333f,
                  "Places oscillator A one octave below its neutral coarse setting");
    }
    if (containsAny(text, {"lead", "wide", "stereo", "anthem"}))
    {
        addChange(proposal, currentParameters, parameterIds::oscillatorAUnison, 0.43f,
                  "Uses four fixed unison lanes for a wider lead body");
        addChange(proposal, currentParameters, parameterIds::oscillatorASpread, 0.72f,
                  "Widens the unison image while retaining a centered fundamental");
        addChange(proposal, currentParameters, parameterIds::oscillatorADetune, 0.18f,
                  "Adds restrained detuning rather than an unstable spread");
    }
    if (containsAny(text, {"move", "moving", "pulse", "wobble", "evolving", "animated"}))
    {
        addChange(proposal, currentParameters, parameterIds::chorusBypass, 0.0f,
                  "Enables the bounded chorus stage for audible motion");
        addChange(proposal, currentParameters, parameterIds::chorusRate, 0.22f,
                  "Keeps modulation movement deliberate rather than frantic");
        addChange(proposal, currentParameters, parameterIds::chorusDepth, 0.42f,
                  "Adds a moderate moving stereo offset");
    }
    if (containsAny(text, {"space", "spacious", "ambient", "atmospheric", "reverb"}))
    {
        addChange(proposal, currentParameters, parameterIds::reverbBypass, 0.0f,
                  "Enables the bounded reverb stage for the requested space");
        addChange(proposal, currentParameters, parameterIds::reverbRoomSize, 0.72f,
                  "Creates a larger but still bounded acoustic field");
        addChange(proposal, currentParameters, parameterIds::reverbMix, 0.30f,
                  "Keeps the dry signal intelligible inside the space");
    }
    if (containsAny(text, {"dry", "close", "intimate"}))
    {
        addChange(proposal, currentParameters, parameterIds::reverbBypass, 1.0f,
                  "Keeps the sound dry and close as requested");
        addChange(proposal, currentParameters, parameterIds::delayBypass, 1.0f,
                  "Avoids a delayed tail in the close presentation");
    }
    if (intensity > 0.72f && containsAny(text, {"aggressive", "intense", "hard", "driven"}))
    {
        addChange(proposal, currentParameters, parameterIds::distortionBypass, 0.0f,
                  "Enables controlled saturation for the requested intensity");
        addChange(proposal, currentParameters, parameterIds::distortionDrive, 0.42f,
                  "Adds bounded drive without selecting the maximum range");
        addChange(proposal, currentParameters, parameterIds::distortionMix, 0.28f,
                  "Blends saturation in parallel to preserve note definition");
    }
    if (proposal.changes.empty())
        addChange(proposal, currentParameters, parameterIds::filterCutoff,
                  0.30f + 0.40f * intensity,
                  "Uses the stated intensity to place the sound on a bounded brightness range");
    if (proposal.changes.empty())
        return {juce::Result::fail("Current parameter snapshot cannot satisfy the sound proposal"),
                std::nullopt, false};

    proposal.explanation = "Offline Jarvis mapped the stated role, tone, articulation, movement, space, and intensity to reviewable host parameters";
    if (!hasText(intent.answers.musicalRole)) proposal.assumptions.push_back("Musical role was inferred from the description");
    if (!hasText(intent.answers.timbre)) proposal.assumptions.push_back("Timbre was inferred from the description");
    if (!hasText(intent.answers.articulation)) proposal.assumptions.push_back("Articulation was inferred from the description");
    if (!hasText(intent.answers.movement)) proposal.assumptions.push_back("Movement was inferred from the description");
    if (!hasText(intent.answers.space)) proposal.assumptions.push_back("Space was inferred from the description");
    if (!intent.answers.intensity.has_value()) proposal.assumptions.push_back("Intensity uses a neutral midpoint");
    if (!hasText(intent.answers.genreContext)) proposal.assumptions.push_back("No genre-specific emphasis was requested");
    const auto answered = requiredGuidedAnswerCount - proposal.assumptions.size();
    proposal.confidence = juce::jlimit(0.35f, 0.95f,
        0.45f + 0.5f * static_cast<float>(answered) / static_cast<float>(requiredGuidedAnswerCount));

    if (const auto validation = validateParameterProposal(proposal); validation.failed())
        return {validation, std::nullopt, false};
    AssistantResponse response;
    response.requestId = request.requestId;
    response.target = request.target;
    response.origin = request.origin;
    response.parameterProposal = std::move(proposal);
    response.summary = "Offline Jarvis prepared an explained sound proposal; audition A/B before accepting";
    AssistantRequest effectiveRequest = request;
    effectiveRequest.soundIntent = intent;
    if (const auto validation = validateAssistantExchange(effectiveRequest, response);
        validation.failed())
        return {validation, std::nullopt, false};
    return {juce::Result::ok(), std::move(response), false};
}

MockAssistantProvider::MockAssistantProvider(const OfflineAssistantEngine& engineToUse)
    : engine(engineToUse)
{
}

juce::String MockAssistantProvider::providerId() const { return "folk-park-mock"; }
AssistantOrigin MockAssistantProvider::origin() const noexcept { return AssistantOrigin::mockProvider; }

juce::Result MockAssistantProvider::submit(const AssistantRequest& request, Completion completion)
{
    if (pending.has_value())
        return juce::Result::fail("Mock assistant already has a pending request");
    if (!completion)
        return juce::Result::fail("Mock assistant completion is required");
    if (request.origin != AssistantOrigin::mockProvider)
        return juce::Result::fail("Mock assistant accepts only mock-provider requests");
    if (const auto validation = validateAssistantRequest(request); validation.failed())
        return validation;
    pending = Pending{request, std::move(completion)};
    return juce::Result::ok();
}

void MockAssistantProvider::cancel(const juce::String& requestId)
{
    if (!pending.has_value() || pending->request.requestId != requestId)
        return;
    auto completion = std::move(pending->completion);
    pending.reset();
    completion({juce::Result::fail("Assistant request was cancelled"), std::nullopt, true});
}

void MockAssistantProvider::setCurrentParameters(std::vector<CurrentParameterValue> values)
{
    currentParameters = std::move(values);
}

bool MockAssistantProvider::hasPendingRequest() const noexcept { return pending.has_value(); }

juce::Result MockAssistantProvider::completePending()
{
    if (!pending.has_value())
        return juce::Result::fail("Mock assistant has no pending request");
    auto request = std::move(pending->request);
    auto completion = std::move(pending->completion);
    pending.reset();
    completion(engine.respond(request, currentParameters));
    return juce::Result::ok();
}
}
