#include "Diagnostics.h"

#include <cmath>
#include <limits>

namespace folkpark::diagnostics
{
namespace
{
juce::String boundedTechnicalText(const juce::String& input,
                                  const juce::String& fallback)
{
    auto trimmed = input.upToFirstOccurrenceOf("\n", false, false)
                       .upToFirstOccurrenceOf("\r", false, false)
                       .upToFirstOccurrenceOf("/", false, false)
                       .upToFirstOccurrenceOf("\\", false, false)
                       .trim().substring(0, 96);
    if (trimmed.isEmpty())
        return fallback;

    juce::String output;
    output.preallocateBytes(96);
    for (const auto character : trimmed)
    {
        const auto permitted = juce::CharacterFunctions::isLetterOrDigit(character)
            || character == ' ' || character == '.' || character == '-'
            || character == '_' || character == '+' || character == '(' || character == ')';
        output += permitted ? juce::String::charToString(character) : "?";
    }
    return output.isEmpty() ? fallback : output;
}

juce::String boundedUnsigned(std::uint64_t value)
{
    return juce::String(static_cast<juce::int64>(juce::jmin(
        value, static_cast<std::uint64_t>(std::numeric_limits<juce::int64>::max()))));
}
}

juce::String serviceCode(ServiceCode code)
{
    switch (code)
    {
        case ServiceCode::ready: return "ready";
        case ServiceCode::degraded: return "degraded";
        case ServiceCode::unavailable: return "unavailable";
        case ServiceCode::disabled: return "disabled";
        case ServiceCode::missingAssets: return "missing-assets";
    }
    return "unavailable";
}

juce::String formatReport(const Snapshot& snapshot)
{
    const auto sampleRate = std::isfinite(snapshot.sampleRate) && snapshot.sampleRate > 0.0
        ? juce::String(snapshot.sampleRate, 2) : juce::String("unavailable");
    const auto blockSize = snapshot.maximumBlockSize > 0
        ? juce::String(snapshot.maximumBlockSize) : juce::String("unavailable");

    juce::String report;
    report << "folk park diagnostics\n"
           << "schema: 1\n"
           << "product_version: " << boundedTechnicalText(snapshot.productVersion, "unavailable") << "\n"
           << "build_type: " << boundedTechnicalText(snapshot.buildType, "unavailable") << "\n"
           << "architecture: " << boundedTechnicalText(snapshot.architecture, "unavailable") << "\n"
           << "format: " << boundedTechnicalText(snapshot.wrapperFormat, "unavailable") << "\n"
           << "host_name: " << boundedTechnicalText(snapshot.hostName, "unavailable") << "\n"
           << "host_version: " << boundedTechnicalText(snapshot.hostVersion, "unavailable") << "\n"
           << "sample_rate_hz: " << sampleRate << "\n"
           << "maximum_block_size: " << blockSize << "\n"
           << "active_voices: " << juce::jlimit(0, 1024, snapshot.activeVoices) << "\n"
           << "preset_status: " << serviceCode(snapshot.preset) << "\n"
           << "database_status: " << serviceCode(snapshot.database) << "\n"
           << "provider_status: " << serviceCode(snapshot.provider) << "\n"
           << "ui_bridge_status: " << serviceCode(snapshot.uiBridge) << "\n"
           << "fault_nonfinite_output_samples: " << boundedUnsigned(snapshot.nonFiniteOutputSamples) << "\n"
           << "fault_direct_midi_overflows: " << boundedUnsigned(snapshot.directMidiOverflows) << "\n"
           << "fault_preview_midi_overflows: " << boundedUnsigned(snapshot.previewMidiOverflows) << "\n"
           << "fault_rejected_project_states: " << boundedUnsigned(snapshot.rejectedProjectStates) << "\n"
           << "privacy: bounded technical status only; no paths, project names, prompts, audio, or credentials\n";

    jassert(report.getNumBytesAsUTF8() < maximumReportBytes);
    if (report.getNumBytesAsUTF8() >= maximumReportBytes)
        return "folk park diagnostics\nschema: 1\nstatus: report-bound-exceeded\n";
    return report;
}

Preview PreviewSession::create(const Snapshot& snapshot)
{
    current = {juce::Uuid().toString(), formatReport(snapshot)};
    return current;
}

std::optional<juce::String> PreviewSession::textForCopy(
    const juce::String& previewId) const
{
    if (current.id.isEmpty() || previewId != current.id)
        return std::nullopt;
    return current.text;
}

void PreviewSession::clear()
{
    current = {};
}
}
