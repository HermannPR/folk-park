#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <span>

namespace folkpark::synth
{
enum class ModulationSource : std::uint8_t
{
    filterEnvelope,
    auxiliaryEnvelope,
    lfo1,
    lfo2,
    lfo3,
    lfo4,
    velocity,
    note,
    modWheel,
    channelPressure,
    count
};

enum class ModulationDestination : std::uint8_t
{
    oscillatorAPosition,
    oscillatorBPosition,
    oscillatorAPitch,
    oscillatorBPitch,
    oscillatorALevel,
    oscillatorBLevel,
    filterCutoff,
    filterResonance,
    filterDrive,
    amplitude,
    pan,
    subLevel,
    noiseLevel,
    count
};

enum class ModulationCurve : std::uint8_t
{
    linear,
    exponential,
    sCurve,
    count
};

struct ModulationRoute
{
    ModulationSource source = ModulationSource::lfo1;
    ModulationDestination destination = ModulationDestination::oscillatorAPosition;
    float amount = 0.0f;
    ModulationCurve curve = ModulationCurve::linear;
    bool enabled = false;
};

struct ModulationSnapshot
{
    static constexpr std::size_t maximumRoutes = 32;
    std::array<ModulationRoute, maximumRoutes> routes{};
    std::size_t routeCount = 0;
};

struct ModulationSourceDescriptor
{
    ModulationSource source;
    const char* stableId;
    const char* displayName;
    bool bipolar;
    bool globalWhenFreeRunning;
};

struct ModulationDestinationDescriptor
{
    ModulationDestination destination;
    const char* stableId;
    const char* displayName;
    float fullScale;
    const char* units;
    bool smoothed;
};

class ModulationRegistry final
{
public:
    using SourceValues = std::array<float, static_cast<std::size_t>(ModulationSource::count)>;
    using DestinationValues = std::array<float, static_cast<std::size_t>(ModulationDestination::count)>;

    [[nodiscard]] static std::span<const ModulationSourceDescriptor> sources() noexcept;
    [[nodiscard]] static std::span<const ModulationDestinationDescriptor> destinations() noexcept;
    [[nodiscard]] static const ModulationSourceDescriptor* descriptor(ModulationSource source) noexcept;
    [[nodiscard]] static const ModulationDestinationDescriptor* descriptor(
        ModulationDestination destination) noexcept;
    [[nodiscard]] static juce::Result validate(std::span<const ModulationRoute> routes) noexcept;
    [[nodiscard]] static ModulationSnapshot makeSnapshot(std::span<const ModulationRoute> routes) noexcept;
    [[nodiscard]] static DestinationValues evaluate(const ModulationSnapshot& snapshot,
                                                    const SourceValues& values) noexcept;
};

class ModulationExchange final
{
public:
    [[nodiscard]] bool publish(std::span<const ModulationRoute> routes) noexcept;
    void beginAudioBlock() noexcept;
    [[nodiscard]] const ModulationSnapshot& current() const noexcept;
    [[nodiscard]] bool hasPendingSnapshot() const noexcept;

private:
    std::array<ModulationSnapshot, 2> snapshots{};
    std::atomic<int> activeIndex{0};
    std::atomic<int> pendingIndex{-1};
    std::atomic_flag writer = ATOMIC_FLAG_INIT;
};
}
