#include "common/ParameterIds.h"
#include "persistence/Preset.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <iostream>
#include <limits>

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

struct TemporaryDirectory
{
    TemporaryDirectory()
    {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("folk-park-preset-tests", {}, false);
        expect(directory.createDirectory(), "Temporary preset test directory must be created");
    }

    ~TemporaryDirectory()
    {
        if (directory.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
            directory.deleteRecursively(false);
    }

    juce::File directory;
};

folkpark::persistence::PresetDocument makeDocument(const juce::String& name = "Test preset")
{
    auto document = folkpark::persistence::makePresetTemplate(
        "0.1.0", "8ca1788c-080f-4ea0-80a8-d9381084aa20", name);
    document.metadata.author = "folk park tests";
    document.metadata.tags = {"bright", "lead"};
    document.metadata.genre = "house";
    document.metadata.emotion = "bright";
    document.metadata.description = "Deterministic M6 fixture";
    document.parameters.front().normalized = 0.625f;
    document.effects.front().parameters[1].normalized = 0.75f;
    document.modulationRoutes.push_back({
        folkpark::synth::ModulationSource::lfo1,
        folkpark::synth::ModulationDestination::oscillatorAPosition,
        0.35f,
        folkpark::synth::ModulationCurve::sCurve,
        true});
    return document;
}

const folkpark::persistence::ParameterValue* findParameter(
    const folkpark::persistence::PresetDocument& document, const juce::String& id)
{
    for (const auto& parameter : document.parameters)
        if (parameter.id == id)
            return &parameter;
    for (const auto& effect : document.effects)
        for (const auto& parameter : effect.parameters)
            if (parameter.id == id)
                return &parameter;
    return nullptr;
}

bool createSingleCycleWav(const juce::File& file, float phaseOffset = 0.0f)
{
    juce::AudioBuffer<float> buffer(1, 2048);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(0, sample,
                         std::sin(juce::MathConstants<float>::twoPi
                                  * static_cast<float>(sample) / 2048.0f + phaseOffset));
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriter::Options{}.withSampleRate(48000.0)
                                                      .withNumChannels(1)
                                                      .withBitsPerSample(24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

void testCurrentCodecAndUnknownFields()
{
    using namespace folkpark::persistence;
    auto document = makeDocument();
    auto* extension = new juce::DynamicObject();
    extension->setProperty("safeText", "preserved");
    extension->setProperty("count", 2);
    document.unknownTopLevelFields.push_back({"futureMetadata", juce::var(extension)});

    const auto first = PresetCodec::encode(document);
    const auto second = PresetCodec::encode(document);
    expect(first.succeeded(), "Current preset must encode");
    expect(first.canonicalJson == second.canonicalJson,
           "Current preset encoding must be byte deterministic");
    expect(first.canonicalJson.contains("\"schemaVersion\": 2"),
           "Current preset JSON must declare version 2");

    const auto decoded = PresetCodec::decode(first.canonicalJson, makeDocument("Defaults"));
    expect(decoded.succeeded() && !decoded.migrated, "Current preset must decode without migration");
    expect(decoded.document.parameters.size() == 73 && decoded.document.effects.size() == 6,
           "Decoded current preset must retain the exact catalog shape");
    expect(decoded.document.modulationRoutes.size() == 1,
           "Decoded current preset must retain modulation routes");
    expect(decoded.document.unknownTopLevelFields.size() == 1,
           "Unknown top-level metadata must be preserved as inert data");
    expect(decoded.canonicalJson == first.canonicalJson,
           "Decode and re-encode must preserve canonical bytes");

    const auto duplicatePosition = first.canonicalJson.indexOf("\"masterGain\"");
    expect(duplicatePosition >= 0, "Canonical fixture must expose the masterGain key");
    if (duplicatePosition >= 0)
    {
        const auto duplicate = first.canonicalJson.replaceSection(
            duplicatePosition, 0, "\"masterGain\": 0.1,\n    ");
        expect(PresetCodec::decode(duplicate, makeDocument("Defaults")).status.failed(),
               "Duplicate JSON object keys must be rejected before parser overwrite");
    }
}

void testLegacyMigration()
{
    using namespace folkpark::persistence;
    const juce::String legacy = R"json({
      "schemaVersion": 1,
      "productVersion": "0.0.9",
      "metadata": {
        "name": "Legacy bass",
        "author": "Producer",
        "genre": "house",
        "emotion": "dark",
        "description": "Oldest supported fixture"
      },
      "parameters": {"masterGain": 0.25, "distDrive": 0.8},
      "modulationRoutes": [],
      "assets": []
    })json";
    const auto defaults = makeDocument("Migration defaults");
    const auto first = PresetCodec::decode(legacy, defaults);
    const auto second = PresetCodec::decode(legacy, defaults);
    expect(first.succeeded() && first.migrated, "Legacy v1 preset must migrate to v2");
    expect(first.document.schemaVersion == 2
               && first.document.migration.originalSchemaVersion == 1
               && first.document.migration.steps == std::vector<juce::String>{"preset-v1-to-v2"},
           "Legacy migration must record exact provenance");
    expect(first.document.metadata.name == "Legacy bass" && first.document.productVersion == "0.0.9",
           "Legacy migration must retain bounded metadata");
    const auto* master = findParameter(first.document, folkpark::parameterIds::masterGain);
    const auto* drive = findParameter(first.document, folkpark::parameterIds::distortionDrive);
    expect(master != nullptr && std::abs(master->normalized - 0.25f) < 1.0e-6f
               && drive != nullptr && std::abs(drive->normalized - 0.8f) < 1.0e-6f,
           "Legacy migration must apply known normalized parameters over deterministic defaults");
    expect(first.document.metadata.id == second.document.metadata.id
               && first.canonicalJson == second.canonicalJson,
           "Legacy migration must not use time, randomness, or machine state");

    const auto fixtureRoot = juce::File(FOLK_PARK_SOURCE_DIR)
                                 .getChildFile("tests/fixtures/presets");
    const auto fixture = PresetCodec::decode(
        fixtureRoot.getChildFile("legacy-v1.folkparkpreset").loadFileAsString(), defaults);
    expect(fixture.succeeded() && fixture.migrated,
           "Checked-in oldest-supported fixture must migrate successfully");
    expect(PresetCodec::decode(
               fixtureRoot.getChildFile("malformed.folkparkpreset").loadFileAsString(), defaults)
               .status.failed(),
           "Checked-in malformed fixture must be rejected");
}

void testAdversarialCodecBoundaries()
{
    using namespace folkpark::persistence;
    const auto defaults = makeDocument("Defaults");
    expect(PresetCodec::decode("{broken", defaults).status.failed(),
           "Malformed JSON must be rejected");
    expect(PresetCodec::decode("{\"schemaVersion\":99}", defaults).status.failed(),
           "Future required schema versions must be rejected");

    juce::String deep;
    for (int index = 0; index < maximumJsonDepth + 2; ++index) deep += "[";
    for (int index = 0; index < maximumJsonDepth + 2; ++index) deep += "]";
    expect(PresetCodec::decode(deep, defaults).status.failed(),
           "JSON nesting beyond the pre-parse bound must be rejected");

    const auto oversized = juce::String::repeatedString("x", static_cast<int>(maximumPresetBytes + 1));
    expect(PresetCodec::decode(oversized, defaults).status.failed(),
           "Preset JSON beyond 1 MiB must be rejected before parsing");

    auto incomplete = makeDocument();
    incomplete.parameters.pop_back();
    expect(PresetCodec::encode(incomplete).status.failed(),
           "A current preset missing one stable parameter must be rejected");
    auto nonFinite = makeDocument();
    nonFinite.parameters.front().normalized = std::numeric_limits<float>::infinity();
    expect(PresetCodec::encode(nonFinite).status.failed(),
           "A non-finite parameter must be rejected");
    auto invalidVersion = makeDocument();
    invalidVersion.productVersion = "version one";
    expect(PresetCodec::encode(invalidVersion).status.failed(),
           "Product version must use the frozen bounded semantic-version form");

    auto traversal = makeDocument();
    const juce::String hash = juce::String::repeatedString("a", 64);
    traversal.assets.push_back({AssetKind::wavetableSource, AssetSlot::oscillatorA, hash,
                                "assets/" + hash + ".wav", 128, "table.wav", hash});
    const auto validReference = PresetCodec::encode(traversal);
    expect(validReference.succeeded(), "A syntactically valid asset reference must encode");
    const auto malicious = validReference.canonicalJson.replace(
        "assets/" + hash + ".wav", "../" + hash + ".wav");
    expect(PresetCodec::decode(malicious, defaults).status.failed(),
           "Preset path traversal must be rejected transactionally");
}

void testAtomicPresetStore()
{
    using namespace folkpark::persistence;
    TemporaryDirectory temporary;
    const auto destination = temporary.directory.getChildFile("Safe Preset.folkparkpreset");
    auto original = makeDocument("Original");
    expect(PresetStore::save(original, destination, false).wasOk(),
           "A valid new preset must save atomically");
    const auto originalBytes = destination.loadFileAsString();
    expect(!originalBytes.isEmpty(), "Saved preset must contain canonical bytes");

    auto replacement = original;
    replacement.metadata.description = "Authorized replacement";
    expect(PresetStore::save(replacement, destination, false).failed(),
           "Existing preset must not be overwritten without authorization");
    expect(destination.loadFileAsString() == originalBytes,
           "Denied overwrite must preserve existing bytes");

    auto invalid = replacement;
    invalid.effects.pop_back();
    expect(PresetStore::save(invalid, destination, true).failed(),
           "Invalid replacement must fail before destination mutation");
    expect(destination.loadFileAsString() == originalBytes,
           "Failed replacement validation must preserve existing bytes");
    expect(PresetStore::save(replacement, destination, true).wasOk(),
           "Explicitly authorized valid replacement must succeed");
    expect(destination.loadFileAsString() != originalBytes,
           "Authorized replacement must publish new canonical bytes");

    const auto loaded = PresetStore::load(destination, makeDocument("Defaults"), temporary.directory);
    expect(loaded.readyToApply() && loaded.document.metadata.description == "Authorized replacement",
           "Saved preset must reopen as a complete ready-to-apply candidate");
    expect(sanitisePresetFilename("../../My <Unsafe>: Lead.folkparkpreset")
               == "My Unsafe Lead.folkparkpreset",
           "Preset filename sanitizer must remove traversal and unsafe punctuation");
}

void testContentAddressedAssetsAndRecovery()
{
    using namespace folkpark::persistence;
    TemporaryDirectory temporary;
    const auto source = temporary.directory.getChildFile("user-table.wav");
    const auto wrong = temporary.directory.getChildFile("wrong-table.wav");
    expect(createSingleCycleWav(source), "Valid source WAV fixture must be written");
    expect(createSingleCycleWav(wrong, 0.5f), "Different source WAV fixture must be written");

    const auto presetRoot = temporary.directory.getChildFile("library");
    AssetReference reference;
    expect(PresetAssetStore::importWavetableSource(source, presetRoot, AssetSlot::oscillatorA,
                                                   reference).wasOk(),
           "Validated user WAV must enter content-addressed storage");
    expect(reference.sha256 == juce::SHA256(source).toHexString().toLowerCase()
               && reference.relativePath == "assets/" + reference.sha256 + ".wav",
           "Imported asset reference must use its lowercase content hash");

    auto document = makeDocument();
    document.assets.push_back(reference);
    expect(PresetAssetStore::validate(document, presetRoot).ready(),
           "Installed content-addressed asset must validate and decode");

    const auto installed = presetRoot.getChildFile(reference.relativePath);
    const auto retained = temporary.directory.getChildFile("retained-source.wav");
    expect(installed.moveFileTo(retained), "Test must isolate the installed asset as missing");
    const auto missing = PresetAssetStore::validate(document, presetRoot);
    expect(missing.status.wasOk() && missing.missing.size() == 1 && !missing.ready(),
           "Missing assets must be reported without treating the preset as apply-ready");
    expect(PresetAssetStore::relink(reference, wrong, presetRoot).failed(),
           "Recovery must reject a user-selected file with the wrong hash");
    expect(PresetAssetStore::relink(reference, source, presetRoot).wasOk(),
           "Recovery must install an explicitly selected matching asset");
    expect(PresetAssetStore::validate(document, presetRoot).ready(),
           "Relinked matching asset must restore apply readiness");

    const auto encoded = PresetCodec::encode(document);
    const auto presetFile = presetRoot.getChildFile("Missing Asset.folkparkpreset");
    expect(presetFile.replaceWithText(encoded.canonicalJson, false, false, "\n"),
           "Missing-asset fixture preset must be written");
    expect(presetRoot.getChildFile(reference.relativePath).moveFileTo(retained),
           "Installed asset must be removed from the isolated fixture root");
    const auto load = PresetStore::load(presetFile, makeDocument("Defaults"), presetRoot);
    expect(load.status.wasOk() && load.missingAssets.size() == 1 && !load.readyToApply(),
           "Preset load must return recoverable metadata but block apply while an asset is missing");

    const auto linkedRoot = temporary.directory.getChildFile("linked-library");
    const auto externalAssets = temporary.directory.getChildFile("external-assets");
    expect(linkedRoot.createDirectory() && externalAssets.createDirectory(),
           "Symlink-boundary fixture directories must be created");
    const auto assetsLink = linkedRoot.getChildFile("assets");
    if (externalAssets.createSymbolicLink(assetsLink, false))
    {
        const auto linkedValidation = PresetAssetStore::validate(document, linkedRoot);
        expect(linkedValidation.status.failed(),
               "An asset-directory symlink must not escape the selected preset root");
    }
}
}

int main()
{
    testCurrentCodecAndUnknownFields();
    testLegacyMigration();
    testAdversarialCodecBoundaries();
    testAtomicPresetStore();
    testContentAddressedAssetsAndRecovery();
    if (failures == 0)
    {
        std::cout << "folk park preset persistence tests passed\n";
        return 0;
    }
    std::cerr << failures << " preset persistence test(s) failed\n";
    return 1;
}
