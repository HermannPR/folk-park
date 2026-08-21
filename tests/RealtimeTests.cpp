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
    if (replacement == nullptr || !engine.publishWavetable(0, *replacement))
    {
        std::cerr << "FAIL: replacement wavetable must queue before the measured audio block\n";
        return 1;
    }
    const ModulationRoute route{ModulationSource::lfo1, ModulationDestination::oscillatorAPosition,
                                0.75f, ModulationCurve::sCurve, true};
    if (!engine.publishModulationRoutes(std::span{&route, 1}))
    {
        std::cerr << "FAIL: modulation snapshot must queue before the measured audio block\n";
        return 1;
    }

    allocationProbe::count.store(0, std::memory_order_relaxed);
    allocationProbe::tracking.store(true, std::memory_order_release);
    for (int block = 0; block < 32; ++block)
        engine.process(audio, midi, parameters);
    allocationProbe::tracking.store(false, std::memory_order_release);

    const auto allocations = allocationProbe::count.load(std::memory_order_relaxed);
    if (allocations != 0)
    {
        std::cerr << "FAIL: measured audio rendering allocated " << allocations << " time(s)\n";
        return 1;
    }
    std::cout << "PASS: 32 audio blocks, including atomic wavetable/matrix activation and table crossfade, allocated 0 times\n";
    return 0;
}
