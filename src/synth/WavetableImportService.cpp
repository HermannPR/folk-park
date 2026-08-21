#include "WavetableImportService.h"

namespace folkpark::synth
{
WavetableImportService::WavetableImportService(Publisher newPublisher)
    : publisher(std::move(newPublisher))
{
}

WavetableImportService::~WavetableImportService()
{
    shuttingDown.store(true, std::memory_order_release);
    generation.fetch_add(1, std::memory_order_acq_rel);
    worker.removeAllJobs(true, 30000);
}

juce::Result WavetableImportService::request(const juce::File& file,
                                             int oscillatorIndex,
                                             int requestedCycleLength)
{
    if (oscillatorIndex < 0 || oscillatorIndex > 1)
        return juce::Result::fail("Wavetable target must be oscillator A or B");
    if (!file.existsAsFile())
        return juce::Result::fail("Selected WAV file does not exist");

    std::uint64_t requestGeneration = 0;
    {
        const std::lock_guard lock(stateMutex);
        if (state.status == Status::processing || state.status == Status::awaitingConfirmation)
            return juce::Result::fail("Finish or cancel the current wavetable import first");
        pendingBank.reset();
        state = {};
        state.status = Status::processing;
        state.oscillatorIndex = oscillatorIndex;
        state.message = "Converting and building mip levels off the audio thread";
        requestGeneration = generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    worker.addJob(std::function<void()>{[this, file, oscillatorIndex, requestedCycleLength, requestGeneration]
    {
        auto result = converter.convertWavFile(file, requestedCycleLength);
        if (shuttingDown.load(std::memory_order_acquire)
            || generation.load(std::memory_order_acquire) != requestGeneration)
            return;

        const std::lock_guard lock(stateMutex);
        if (result.succeeded())
        {
            state.status = Status::awaitingConfirmation;
            state.oscillatorIndex = oscillatorIndex;
            state.message = "Preview ready - confirm to load this immutable bank";
            state.metadata = result.metadata;
            state.preview = result.preview;
            pendingBank = std::move(result.bank);
        }
        else
        {
            state.status = Status::failed;
            state.oscillatorIndex = oscillatorIndex;
            state.message = result.status.getErrorMessage();
            pendingBank.reset();
        }
    }});
    return juce::Result::ok();
}

juce::Result WavetableImportService::confirm()
{
    const std::lock_guard lock(stateMutex);
    if (state.status != Status::awaitingConfirmation || pendingBank == nullptr)
        return juce::Result::fail("No converted wavetable is waiting for confirmation");
    if (!publisher(state.oscillatorIndex, *pendingBank))
    {
        state.message = "Audio exchange is busy; confirm again after the pending crossfade";
        return juce::Result::fail(state.message);
    }
    pendingBank.reset();
    state.status = Status::loaded;
    state.message = "Wavetable accepted and queued for a click-safe block-boundary swap";
    return juce::Result::ok();
}

void WavetableImportService::cancel()
{
    generation.fetch_add(1, std::memory_order_acq_rel);
    const std::lock_guard lock(stateMutex);
    pendingBank.reset();
    state.status = Status::cancelled;
    state.message = "Wavetable import cancelled without changing either oscillator";
}

WavetableImportService::Snapshot WavetableImportService::getSnapshot() const
{
    const std::lock_guard lock(stateMutex);
    return state;
}
}
