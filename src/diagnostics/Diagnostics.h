#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace folkpark::diagnostics
{
constexpr std::size_t maximumReportBytes = 4096;

enum class ServiceCode
{
    ready,
    degraded,
    unavailable,
    disabled,
    missingAssets
};

struct Snapshot
{
    juce::String productVersion;
    juce::String buildType;
    juce::String architecture;
    juce::String wrapperFormat;
    juce::String hostName;
    juce::String hostVersion;
    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    int activeVoices = 0;
    ServiceCode preset = ServiceCode::unavailable;
    ServiceCode database = ServiceCode::unavailable;
    ServiceCode provider = ServiceCode::disabled;
    ServiceCode uiBridge = ServiceCode::ready;
    std::uint64_t nonFiniteOutputSamples = 0;
    std::uint64_t directMidiOverflows = 0;
    std::uint64_t previewMidiOverflows = 0;
    std::uint64_t rejectedProjectStates = 0;
};

struct Preview
{
    juce::String id;
    juce::String text;
};

[[nodiscard]] juce::String serviceCode(ServiceCode code);
[[nodiscard]] juce::String formatReport(const Snapshot& snapshot);

class PreviewSession final
{
public:
    [[nodiscard]] Preview create(const Snapshot& snapshot);
    [[nodiscard]] std::optional<juce::String> textForCopy(const juce::String& previewId) const;
    void clear();

private:
    Preview current;
};
}
