#include "effects/EffectChain.h"
#include "midi/Composition.h"
#include "midi/MidiDelivery.h"
#include "midi/PreviewMidi.h"
#include "synth/Modulation.h"
#include "synth/SynthEngine.h"
#include "synth/WavetableBank.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>

namespace allocationProbe
{
std::atomic<bool> tracking{false};
std::atomic<std::uint64_t> count{0};

static void record() noexcept
{
    if (tracking.load(std::memory_order_relaxed))
        count.fetch_add(1, std::memory_order_relaxed);
}

static void* allocate(std::size_t size)
{
    record();
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc{};
}

static void* allocateAligned(std::size_t size, std::size_t alignment)
{
    record();
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) == 0)
        return memory;
    throw std::bad_alloc{};
}
}

void* operator new(std::size_t size) { return allocationProbe::allocate(size); }
void* operator new[](std::size_t size) { return allocationProbe::allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocationProbe::allocate(size); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocationProbe::allocate(size); } catch (...) { return nullptr; }
}
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }

void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocationProbe::allocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocationProbe::allocateAligned(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try { return allocationProbe::allocateAligned(size, static_cast<std::size_t>(alignment)); }
    catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try { return allocationProbe::allocateAligned(size, static_cast<std::size_t>(alignment)); }
    catch (...) { return nullptr; }
}
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }

int main()
{
    using namespace folkpark::midi;
    using namespace folkpark::synth;
    constexpr auto blockSize = 512;
    SynthEngine engine;
    ParameterSnapshot parameters;
    parameters.oscillatorA.unisonVoices = SynthEngine::maximumUnisonVoices;
    parameters.oscillatorB.unisonVoices = SynthEngine::maximumUnisonVoices;
    parameters.oscillatorB.levelDb = -9.0f;
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    engine.prepare(48000.0, blockSize);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(110)), 0);
    engine.process(audio, midi, parameters);
    midi.clear();

    const auto replacement = WavetableBank::createBuiltIn();
    const ModulationRoute route{ModulationSource::lfo1, ModulationDestination::oscillatorAPosition,
                                0.75f, ModulationCurve::sCurve, true};
    if (replacement == nullptr
        || !engine.publishPresetSnapshot(*replacement, *replacement, std::span{&route, 1}))
    {
        std::cerr << "FAIL: atomic preset snapshot must queue before the measured audio block\n";
        return 1;
    }

    MusicIntent directIntent;
    directIntent.seed = 42;
    directIntent.requestId = deterministicUuid(directIntent.seed, "realtime-direct-midi");
    directIntent.partCount = 1;
    directIntent.parts[0] = PartType::melody;
    directIntent.constraints.maxPolyphony = 1;
    directIntent.constraints.maximumEvents = 4;
    GeneratedClip directClip;
    directClip.id = deterministicUuid(directIntent.seed, "realtime-direct-midi-clip");
    directClip.part = PartType::melody;
    directClip.lengthTicks = compositionPpq * 4;
    directClip.tempoBpm = directIntent.tempoBpm;
    directClip.timeSignature = directIntent.timeSignature;
    directClip.key = directIntent.key;
    directClip.scale = directIntent.scale;
    directClip.seed = directIntent.seed;
    directClip.createdUnixMs = 1;
    directClip.events.push_back({0, compositionPpq, 60, 100, 1, 1.0f, Articulation::normal});
    const CompositionBundle directBundle{directIntent, {directClip}};
    DirectMidiPlayer directPlayer;
    if (validateBundle(directBundle).failed() || directPlayer.publish(directBundle).failed())
    {
        std::cerr << "FAIL: direct MIDI allocation fixture must publish before measurement\n";
        return 1;
    }
    juce::MidiBuffer directOutput;
    directOutput.ensureSize(2048);
    PreviewMidiQueue previewQueue;
    if (!previewQueue.enqueueNoteOn(67, 100))
    {
        std::cerr << "FAIL: preview MIDI allocation fixture must enqueue before measurement\n";
        return 1;
    }
    folkpark::effects::EffectChain effects;
    folkpark::effects::Parameters effectParameters;
    effectParameters.distortionBypass = false;
    effectParameters.chorusBypass = false;
    effectParameters.delayBypass = false;
    effectParameters.reverbBypass = false;
    effectParameters.compressorBypass = false;
    effectParameters.eqBypass = false;
    effects.prepare(48000.0, blockSize);

    allocationProbe::count.store(0, std::memory_order_relaxed);
    allocationProbe::tracking.store(true, std::memory_order_release);
    for (int block = 0; block < 32; ++block)
    {
        engine.process(audio, midi, parameters);
        effects.process(audio, effectParameters);
        directOutput.clear();
        const auto previewEvents = previewQueue.renderBlock(directOutput);
        juce::ignoreUnused(previewEvents);
        const auto rendered = directPlayer.renderBlock(directOutput, blockSize, 48000.0);
        if (rendered.overflow)
        {
            allocationProbe::tracking.store(false, std::memory_order_release);
            std::cerr << "FAIL: direct MIDI overflowed during allocation measurement\n";
            return 1;
        }
    }
    allocationProbe::tracking.store(false, std::memory_order_release);

    const auto allocations = allocationProbe::count.load(std::memory_order_relaxed);
    if (allocations != 0)
    {
        std::cerr << "FAIL: measured audio rendering allocated " << allocations << " time(s)\n";
        return 1;
    }
    auto invalidRoute = route;
    invalidRoute.destination = static_cast<ModulationDestination>(255);
    if (engine.publishPresetSnapshot(*replacement, *replacement,
                                     std::span{&invalidRoute, 1}))
    {
        std::cerr << "FAIL: malformed preset publication must be rejected before live mutation\n";
        return 1;
    }
    engine.process(audio, midi, parameters);
    const auto& retainedRoutes = engine.getActiveModulationSnapshot();
    if (retainedRoutes.routeCount != 1
        || retainedRoutes.routes[0].destination != route.destination)
    {
        std::cerr << "FAIL: rejected preset publication changed the active modulation snapshot\n";
        return 1;
    }
    std::cout << "PASS: 32 audio blocks, including atomic preset activation, six effects, direct MIDI, and preview keyboard, allocated 0 times\n";
    return 0;
}
