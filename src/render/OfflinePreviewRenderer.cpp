#include "OfflinePreviewRenderer.h"

#include "midi/MidiDelivery.h"

#include <algorithm>
#include <cmath>

namespace folkpark::render
{
namespace
{
double compositionDurationSeconds(const midi::CompositionBundle& bundle) noexcept
{
    auto duration = 0.0;
    for (const auto& clip : bundle.clips)
    {
        const auto secondsPerTick = 60.0 / (clip.tempoBpm * static_cast<double>(clip.ppq));
        duration = std::max(duration, static_cast<double>(clip.lengthTicks) * secondsPerTick);
    }
    return duration;
}

juce::File temporarySiblingFor(const juce::File& destination)
{
    return destination.getSiblingFile("." + destination.getFileNameWithoutExtension()
                                      + "-" + juce::Uuid().toDashedString() + ".tmp.wav");
}
}

WavRenderResult OfflinePreviewRenderer::render(const midi::CompositionBundle& accepted,
                                                const OfflinePreviewSnapshot& snapshot,
                                                const juce::File& destination,
                                                bool allowOverwrite,
                                                double sampleRate,
                                                CancellationCheck cancelled) const
{
    WavRenderResult result;
    if (const auto validation = midi::validateBundle(accepted); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (snapshot.wavetableA == nullptr || snapshot.wavetableB == nullptr
        || !snapshot.wavetableA->isFiniteAndNormalised()
        || !snapshot.wavetableB->isFiniteAndNormalised())
    {
        result.status = juce::Result::fail("Offline preview requires two valid immutable wavetables");
        return result;
    }
    const auto routes = std::span{snapshot.modulation.routes}.first(snapshot.modulation.routeCount);
    if (const auto validation = synth::ModulationRegistry::validate(routes); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (destination == juce::File{} || destination.isDirectory()
        || !destination.getParentDirectory().isDirectory())
    {
        result.status = juce::Result::fail("Offline WAV destination is invalid");
        return result;
    }
    if (destination.existsAsFile() && !allowOverwrite)
    {
        result.status = juce::Result::fail("Offline WAV destination already exists");
        return result;
    }
    if (!std::isfinite(sampleRate) || (sampleRate != 44100.0 && sampleRate != 48000.0
                                       && sampleRate != 96000.0))
    {
        result.status = juce::Result::fail("Offline WAV sample rate must be 44.1, 48, or 96 kHz");
        return result;
    }
    const auto musicalSeconds = compositionDurationSeconds(accepted);
    const auto durationSeconds = musicalSeconds + tailSeconds;
    if (!std::isfinite(durationSeconds) || durationSeconds <= tailSeconds
        || durationSeconds > maximumDurationSeconds)
    {
        result.status = juce::Result::fail("Offline WAV duration is outside the 15-minute bound");
        return result;
    }
    result.sampleRate = sampleRate;
    result.durationSeconds = durationSeconds;
    result.sampleCount = static_cast<std::int64_t>(std::ceil(durationSeconds * sampleRate));

    const auto temporary = temporarySiblingFor(destination);
    const auto cleanTemporary = [&temporary]
    {
        if (temporary.existsAsFile())
            temporary.deleteFile();
    };
    std::unique_ptr<juce::OutputStream> stream = temporary.createOutputStream();
    if (stream == nullptr)
    {
        result.status = juce::Result::fail("Offline WAV temporary file could not be opened");
        return result;
    }
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream,
        juce::AudioFormatWriterOptions{}.withSampleRate(sampleRate)
                                         .withNumChannels(2)
                                         .withBitsPerSample(24));
    if (writer == nullptr)
    {
        cleanTemporary();
        result.status = juce::Result::fail("Offline WAV writer could not be created");
        return result;
    }

    auto engine = std::make_unique<synth::SynthEngine>();
    auto effects = std::make_unique<effects::EffectChain>();
    auto midiPlayer = std::make_unique<midi::DirectMidiPlayer>();
    engine->prepare(sampleRate, blockSize);
    effects->prepare(sampleRate, blockSize);
    if (!engine->publishWavetable(0, *snapshot.wavetableA)
        || !engine->publishWavetable(1, *snapshot.wavetableB)
        || !engine->publishModulationRoutes(routes))
    {
        writer.reset();
        cleanTemporary();
        result.status = juce::Result::fail("Offline synth snapshot could not be published");
        return result;
    }

    // Consume immutable exchanges before any musical event, then reset only voice/LFO state.
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    engine->process(audio, midi, snapshot.synthParameters);
    engine->reset();
    effects->reset();
    if (const auto publication = midiPlayer->publish(accepted); publication.failed())
    {
        writer.reset();
        cleanTemporary();
        result.status = publication;
        return result;
    }

    const auto masterGain = juce::Decibels::decibelsToGain(
        juce::jlimit(-60.0f, 6.0f, std::isfinite(snapshot.masterGainDb)
                                         ? snapshot.masterGainDb : -12.0f), -60.0f);
    auto written = std::int64_t{0};
    while (written < result.sampleCount)
    {
        if (cancelled && cancelled())
        {
            writer.reset();
            cleanTemporary();
            result.status = juce::Result::fail("Offline WAV rendering was cancelled");
            return result;
        }
        const auto count = static_cast<int>(std::min<std::int64_t>(blockSize,
                                                                   result.sampleCount - written));
        audio.setSize(2, count, false, false, true);
        midi.clear();
        midi.ensureSize(4096);
        const auto midiResult = midiPlayer->renderBlock(midi, count, sampleRate);
        if (midiResult.overflow)
        {
            writer.reset();
            cleanTemporary();
            result.status = juce::Result::fail("Offline MIDI exceeded its per-block safety bound");
            return result;
        }
        engine->process(audio, midi, snapshot.synthParameters);
        effects->process(audio, snapshot.effectParameters);
        audio.applyGain(masterGain);
        if (!writer->writeFromAudioSampleBuffer(audio, 0, count))
        {
            writer.reset();
            cleanTemporary();
            result.status = juce::Result::fail("Offline WAV write failed before completion");
            return result;
        }
        written += count;
    }
    writer.reset();

    if (const auto validation = validateWav(temporary, result.sampleCount, sampleRate);
        validation.failed())
    {
        cleanTemporary();
        result.status = validation;
        return result;
    }
    if (!temporary.replaceFileIn(destination))
    {
        cleanTemporary();
        result.status = juce::Result::fail("Validated offline WAV could not replace its destination");
        return result;
    }
    result.status = juce::Result::ok();
    return result;
}

juce::Result OfflinePreviewRenderer::validateWav(const juce::File& file,
                                                  std::int64_t expectedSamples,
                                                  double expectedSampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (reader == nullptr || !reader->getFormatName().startsWithIgnoreCase("WAV"))
        return juce::Result::fail("Rendered preview is not a readable WAV file");
    if (reader->numChannels != 2 || reader->bitsPerSample != 24)
        return juce::Result::fail("Rendered WAV must be stereo 24-bit PCM");
    if (std::abs(reader->sampleRate - expectedSampleRate) > 0.5)
        return juce::Result::fail("Rendered WAV sample rate differs from the requested rate");
    if (reader->lengthInSamples != expectedSamples)
        return juce::Result::fail("Rendered WAV length differs from the deterministic render length");
    return juce::Result::ok();
}

OfflinePreviewService::~OfflinePreviewService()
{
    shuttingDown.store(true, std::memory_order_release);
    generation.fetch_add(1, std::memory_order_acq_rel);
    worker.removeAllJobs(true, 30000);
}

juce::Result OfflinePreviewService::request(midi::CompositionBundle accepted,
                                             OfflinePreviewSnapshot snapshot,
                                             juce::File destination,
                                             bool allowOverwrite)
{
    std::uint64_t requestGeneration = 0;
    {
        const std::lock_guard lock(stateMutex);
        if (state.status == Status::rendering)
            return juce::Result::fail("An accepted WAV render is already running");
        requestGeneration = generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        state = {};
        state.status = Status::rendering;
        state.message = "Rendering the accepted composition in an isolated synth";
        state.destination = destination.getFullPathName();
    }
    worker.addJob(std::function<void()>{[this, acceptedCopy = std::move(accepted),
                                        snapshotCopy = std::move(snapshot), destination,
                                        allowOverwrite, requestGeneration]() mutable
    {
        const auto cancelled = [this, requestGeneration]
        {
            return shuttingDown.load(std::memory_order_acquire)
                || generation.load(std::memory_order_acquire) != requestGeneration;
        };
        const auto rendered = renderer.render(acceptedCopy, snapshotCopy, destination, allowOverwrite,
                                              OfflinePreviewRenderer::defaultSampleRate, cancelled);
        if (shuttingDown.load(std::memory_order_acquire)
            || generation.load(std::memory_order_acquire) != requestGeneration)
            return;
        const std::lock_guard lock(stateMutex);
        state.sampleCount = rendered.sampleCount;
        state.sampleRate = rendered.sampleRate;
        state.durationSeconds = rendered.durationSeconds;
        if (rendered.succeeded())
        {
            state.status = Status::rendered;
            state.message = "Accepted composition rendered and validated as a 24-bit WAV";
        }
        else
        {
            state.status = Status::failed;
            state.message = rendered.status.getErrorMessage();
        }
    }});
    return juce::Result::ok();
}

void OfflinePreviewService::cancel()
{
    generation.fetch_add(1, std::memory_order_acq_rel);
    const std::lock_guard lock(stateMutex);
    if (state.status == Status::rendering)
    {
        state.status = Status::cancelled;
        state.message = "Offline WAV rendering cancelled; no destination was replaced";
    }
}

OfflinePreviewService::Snapshot OfflinePreviewService::getSnapshot() const
{
    const std::lock_guard lock(stateMutex);
    return state;
}
}
