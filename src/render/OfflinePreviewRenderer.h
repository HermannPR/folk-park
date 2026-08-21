#pragma once

#include "effects/EffectChain.h"
#include "midi/Composition.h"
#include "synth/Modulation.h"
#include "synth/SynthEngine.h"
#include "synth/WavetableBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace folkpark::render
{
struct OfflinePreviewSnapshot
{
    synth::ParameterSnapshot synthParameters;
    effects::Parameters effectParameters;
    synth::ModulationSnapshot modulation;
    std::shared_ptr<const synth::WavetableBank> wavetableA;
    std::shared_ptr<const synth::WavetableBank> wavetableB;
    float masterGainDb = -12.0f;
};

struct WavRenderResult
{
    juce::Result status = juce::Result::fail("Offline WAV rendering did not run");
    std::int64_t sampleCount = 0;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

class OfflinePreviewRenderer final
{
public:
    static constexpr double defaultSampleRate = 48000.0;
    static constexpr int blockSize = 512;
    static constexpr double tailSeconds = 12.0;
    static constexpr double maximumDurationSeconds = 900.0;

    using CancellationCheck = std::function<bool()>;

    [[nodiscard]] WavRenderResult render(const midi::CompositionBundle& accepted,
                                         const OfflinePreviewSnapshot& snapshot,
                                         const juce::File& destination,
                                         bool allowOverwrite = false,
                                         double sampleRate = defaultSampleRate,
                                         CancellationCheck cancelled = {}) const;
    [[nodiscard]] static juce::Result validateWav(const juce::File& file,
                                                  std::int64_t expectedSamples,
                                                  double expectedSampleRate);
};

class OfflinePreviewService final
{
public:
    enum class Status
    {
        idle,
        rendering,
        rendered,
        failed,
        cancelled
    };

    struct Snapshot
    {
        Status status = Status::idle;
        juce::String message{"No accepted WAV has been rendered"};
        juce::String destination;
        std::int64_t sampleCount = 0;
        double sampleRate = 0.0;
        double durationSeconds = 0.0;
    };

    OfflinePreviewService() = default;
    ~OfflinePreviewService();

    [[nodiscard]] juce::Result request(midi::CompositionBundle accepted,
                                       OfflinePreviewSnapshot snapshot,
                                       juce::File destination,
                                       bool allowOverwrite);
    void cancel();
    [[nodiscard]] Snapshot getSnapshot() const;

private:
    OfflinePreviewRenderer renderer;
    juce::ThreadPool worker{1};
    mutable std::mutex stateMutex;
    Snapshot state;
    std::atomic<std::uint64_t> generation{0};
    std::atomic<bool> shuttingDown{false};
};
}
