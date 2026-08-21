#include "Modulation.h"

#include <algorithm>
#include <cmath>

namespace folkpark::synth
{
namespace
{
constexpr std::array sourceDescriptors{
    ModulationSourceDescriptor{ModulationSource::filterEnvelope, "filterEnv", "Filter Envelope", false, false},
    ModulationSourceDescriptor{ModulationSource::auxiliaryEnvelope, "auxEnv", "Auxiliary Envelope", false, false},
    ModulationSourceDescriptor{ModulationSource::lfo1, "lfo1", "LFO 1", true, true},
    ModulationSourceDescriptor{ModulationSource::lfo2, "lfo2", "LFO 2", true, true},
    ModulationSourceDescriptor{ModulationSource::lfo3, "lfo3", "LFO 3", true, true},
    ModulationSourceDescriptor{ModulationSource::lfo4, "lfo4", "LFO 4", true, true},
    ModulationSourceDescriptor{ModulationSource::velocity, "velocity", "Velocity", false, false},
    ModulationSourceDescriptor{ModulationSource::note, "note", "Note", false, false},
    ModulationSourceDescriptor{ModulationSource::modWheel, "modWheel", "Mod Wheel", false, true},
    ModulationSourceDescriptor{ModulationSource::channelPressure, "channelPressure", "Channel Pressure", false, true},
};

constexpr std::array destinationDescriptors{
    ModulationDestinationDescriptor{ModulationDestination::oscillatorAPosition,
                                    "oscAPosition", "Oscillator A Position", 1.0f, "ratio", true},
    ModulationDestinationDescriptor{ModulationDestination::oscillatorBPosition,
                                    "oscBPosition", "Oscillator B Position", 1.0f, "ratio", true},
    ModulationDestinationDescriptor{ModulationDestination::oscillatorAPitch,
                                    "oscAPitch", "Oscillator A Pitch", 24.0f, "semitones", true},
    ModulationDestinationDescriptor{ModulationDestination::oscillatorBPitch,
                                    "oscBPitch", "Oscillator B Pitch", 24.0f, "semitones", true},
    ModulationDestinationDescriptor{ModulationDestination::oscillatorALevel,
                                    "oscALevel", "Oscillator A Level", 24.0f, "dB", true},
    ModulationDestinationDescriptor{ModulationDestination::oscillatorBLevel,
                                    "oscBLevel", "Oscillator B Level", 24.0f, "dB", true},
    ModulationDestinationDescriptor{ModulationDestination::filterCutoff,
                                    "filterCutoff", "Filter Cutoff", 8.0f, "octaves", true},
    ModulationDestinationDescriptor{ModulationDestination::filterResonance,
                                    "filterResonance", "Filter Resonance", 0.95f, "ratio", true},
    ModulationDestinationDescriptor{ModulationDestination::filterDrive,
                                    "filterDrive", "Filter Drive", 18.0f, "dB", true},
    ModulationDestinationDescriptor{ModulationDestination::amplitude,
                                    "amplitude", "Amplitude", 1.0f, "ratio", true},
    ModulationDestinationDescriptor{ModulationDestination::pan,
                                    "pan", "Pan", 1.0f, "ratio", true},
    ModulationDestinationDescriptor{ModulationDestination::subLevel,
                                    "subLevel", "Sub Level", 24.0f, "dB", true},
    ModulationDestinationDescriptor{ModulationDestination::noiseLevel,
                                    "noiseLevel", "Noise Level", 24.0f, "dB", true},
};

template <typename Enum>
bool isValidEnum(Enum value, Enum count) noexcept
{
    const auto raw = static_cast<std::underlying_type_t<Enum>>(value);
    return raw >= 0 && raw < static_cast<std::underlying_type_t<Enum>>(count);
}

float applyCurve(float value, ModulationCurve curve, bool bipolar) noexcept
{
    const auto bounded = bipolar ? juce::jlimit(-1.0f, 1.0f, value)
                                 : juce::jlimit(0.0f, 1.0f, value);
    switch (curve)
    {
        case ModulationCurve::linear:
            return bounded;
        case ModulationCurve::exponential:
            return bipolar ? std::copysign(bounded * bounded, bounded) : bounded * bounded;
        case ModulationCurve::sCurve:
        {
            if (bipolar)
            {
                const auto unipolar = 0.5f * (bounded + 1.0f);
                return 2.0f * (unipolar * unipolar * (3.0f - 2.0f * unipolar)) - 1.0f;
            }
            return bounded * bounded * (3.0f - 2.0f * bounded);
        }
        case ModulationCurve::count:
            break;
    }
    return 0.0f;
}
}

std::span<const ModulationSourceDescriptor> ModulationRegistry::sources() noexcept
{
    return sourceDescriptors;
}

std::span<const ModulationDestinationDescriptor> ModulationRegistry::destinations() noexcept
{
    return destinationDescriptors;
}

const ModulationSourceDescriptor* ModulationRegistry::descriptor(ModulationSource source) noexcept
{
    if (!isValidEnum(source, ModulationSource::count))
        return nullptr;
    return &sourceDescriptors[static_cast<std::size_t>(source)];
}

const ModulationDestinationDescriptor* ModulationRegistry::descriptor(
    ModulationDestination destination) noexcept
{
    if (!isValidEnum(destination, ModulationDestination::count))
        return nullptr;
    return &destinationDescriptors[static_cast<std::size_t>(destination)];
}

juce::Result ModulationRegistry::validate(std::span<const ModulationRoute> routes) noexcept
{
    if (routes.size() > ModulationSnapshot::maximumRoutes)
        return juce::Result::fail("Modulation route count exceeds the Release 0.1 bound");

    for (const auto& route : routes)
    {
        if (!isValidEnum(route.source, ModulationSource::count))
            return juce::Result::fail("Modulation source is unsupported");
        if (!isValidEnum(route.destination, ModulationDestination::count))
            return juce::Result::fail("Modulation destination is unsupported");
        if (!isValidEnum(route.curve, ModulationCurve::count))
            return juce::Result::fail("Modulation curve is unsupported");
        if (!std::isfinite(route.amount) || route.amount < -1.0f || route.amount > 1.0f)
            return juce::Result::fail("Modulation amount must be finite and bipolar-normalized");
    }
    return juce::Result::ok();
}

ModulationSnapshot ModulationRegistry::makeSnapshot(std::span<const ModulationRoute> routes) noexcept
{
    ModulationSnapshot snapshot;
    if (validate(routes).failed())
        return snapshot;
    snapshot.routeCount = routes.size();
    std::copy(routes.begin(), routes.end(), snapshot.routes.begin());
    return snapshot;
}

ModulationRegistry::DestinationValues ModulationRegistry::evaluate(const ModulationSnapshot& snapshot,
                                                                    const SourceValues& values) noexcept
{
    DestinationValues destinations{};
    const auto routeCount = std::min(snapshot.routeCount, ModulationSnapshot::maximumRoutes);
    for (std::size_t index = 0; index < routeCount; ++index)
    {
        const auto& route = snapshot.routes[index];
        if (!route.enabled)
            continue;
        const auto* source = descriptor(route.source);
        if (source == nullptr || descriptor(route.destination) == nullptr)
            continue;
        const auto shaped = applyCurve(values[static_cast<std::size_t>(route.source)], route.curve,
                                       source->bipolar);
        auto& destination = destinations[static_cast<std::size_t>(route.destination)];
        destination = juce::jlimit(-1.0f, 1.0f, destination + shaped * route.amount);
    }
    return destinations;
}

bool ModulationExchange::publish(std::span<const ModulationRoute> routes) noexcept
{
    if (ModulationRegistry::validate(routes).failed()
        || writer.test_and_set(std::memory_order_acquire))
        return false;

    const auto releaseWriter = [this]
    {
        writer.clear(std::memory_order_release);
    };

    if (pendingIndex.load(std::memory_order_acquire) >= 0)
    {
        releaseWriter();
        return false;
    }

    const auto inactive = 1 - activeIndex.load(std::memory_order_acquire);
    snapshots[static_cast<std::size_t>(inactive)] = ModulationRegistry::makeSnapshot(routes);
    pendingIndex.store(inactive, std::memory_order_release);
    releaseWriter();
    return true;
}

void ModulationExchange::beginAudioBlock() noexcept
{
    const auto pending = pendingIndex.exchange(-1, std::memory_order_acquire);
    if (pending >= 0)
        activeIndex.store(pending, std::memory_order_release);
}

const ModulationSnapshot& ModulationExchange::current() const noexcept
{
    return snapshots[static_cast<std::size_t>(activeIndex.load(std::memory_order_acquire))];
}

bool ModulationExchange::hasPendingSnapshot() const noexcept
{
    return pendingIndex.load(std::memory_order_acquire) >= 0;
}
}
