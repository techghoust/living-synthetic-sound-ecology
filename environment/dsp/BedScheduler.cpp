#include "BedScheduler.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lsse::environment
{
void BedScheduler::prepare (double sampleRate, std::uint32_t seed) { rate = std::clamp (sampleRate, 8000.0, 384000.0); seedValue = seed; reset(); }
void BedScheduler::reset() {}
std::uint32_t BedScheduler::hash (std::uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; return static_cast<std::uint32_t> (x ^ (x >> 32)); }
float BedScheduler::sampleAt (std::uint64_t absoluteSample, bool generator, float density)
{
    if (! generator) return 0.0f;
    const auto cellSize = static_cast<std::uint64_t> (std::max (1.0, rate * 0.25));
    const auto cell = absoluteSample / cellSize, offset = absoluteSample % cellSize;
    const auto gate = (hash (cell ^ seedValue) & 0xffffu) / 65535.0f;
    if (gate > std::clamp (density, 0.0f, 1.0f)) return 0.0f;
    const auto t = static_cast<double> (offset) / static_cast<double> (cellSize);
    const auto amp = std::sin (std::numbers::pi * t) * 0.12;
    const auto noise = static_cast<std::int32_t> (hash (absoluteSample + seedValue * 131u)) / 2147483648.0;
    return static_cast<float> (amp * noise);
}
BedScheduler::Layers BedScheduler::layersAt (std::uint64_t absoluteSample, bool generator, float density,
                                              float variation, float scale) const
{
    if (! generator) return {};
    const auto d = std::clamp (density, 0.0f, 1.0f);
    const auto v = std::clamp (variation, 0.0f, 1.0f);
    const auto s = std::clamp (scale, 0.0f, 1.0f);
    const auto noise = [&] (std::uint64_t key, std::uint32_t stream) {
        return static_cast<float> (static_cast<std::int32_t> (hash (key ^ (static_cast<std::uint64_t> (seedValue) << 17) ^ stream)) / 2147483648.0);
    };
    const auto slowPhase = 2.0 * std::numbers::pi * static_cast<double> (absoluteSample % static_cast<std::uint64_t> (rate * 19.0)) / (rate * 19.0);
    Layers result;
    result.bed = static_cast<float> ((0.035 + 0.025 * s) * (noise (absoluteSample / 3, 0x11u) * 0.45 + std::sin (slowPhase) * 0.55));
    const auto weatherCellSize = static_cast<std::uint64_t> (std::max (1.0, rate * (0.03 + 0.22 * (1.0 - d))));
    const auto weatherCell = absoluteSample / weatherCellSize;
    const auto weatherOffset = absoluteSample % weatherCellSize;
    const auto weatherGate = (hash (weatherCell ^ seedValue ^ 0x57u) & 0xffffu) / 65535.0f;
    if (weatherGate < d * (0.25f + 0.75f * v))
    {
        const auto t = static_cast<double> (weatherOffset) / weatherCellSize;
        result.weather = static_cast<float> (noise (absoluteSample, 0x57u) * std::sin (std::numbers::pi * t) * 0.11);
    }
    const auto eventCellSize = static_cast<std::uint64_t> (std::max (1.0, rate * (0.18 + 1.2 * (1.0 - d))));
    const auto eventCell = absoluteSample / eventCellSize;
    const auto eventOffset = absoluteSample % eventCellSize;
    const auto eventGate = (hash (eventCell ^ seedValue ^ 0xe7u) & 0xffffu) / 65535.0f;
    if (eventGate < d * 0.55f)
    {
        const auto t = static_cast<double> (eventOffset) / eventCellSize;
        const auto frequency = 140.0 + s * 1100.0 + (hash (eventCell ^ 0x9du) & 255u) * (1.0 + v * 3.0);
        result.events = static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * eventOffset / rate) * std::exp (-7.0 * t) * 0.16);
    }
    return result;
}
}
