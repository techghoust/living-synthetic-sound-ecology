#include "PathEngine.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lsse::motion
{
std::pair<float, float> PathEngine::orbit (double phase)
{
    phase -= std::floor (phase);
    const auto angle = 2.0 * std::numbers::pi * phase;
    return { static_cast<float> (std::sin (angle)), static_cast<float> (std::cos (angle)) };
}
std::pair<float, float> PathEngine::position (int path, double phase, std::uint32_t seed, float randomness)
{
    phase -= std::floor (phase);
    const auto angle = 2.0 * std::numbers::pi * phase;
    if (path == 0) return { static_cast<float> (phase * 2.0 - 1.0), 0.0f };
    if (path == 1) return orbit (phase);
    if (path == 2) return { static_cast<float> (std::sin (angle)), static_cast<float> (-std::abs (std::cos (angle))) };
    if (path == 3) return { static_cast<float> (std::sin (angle)), static_cast<float> (std::sin (angle * 2.0) * 0.75) };
    auto hash = [] (std::uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16); };
    const auto cellPosition = phase * 32.0;
    const auto cell = static_cast<std::uint32_t> (cellPosition);
    const auto fraction = static_cast<float> (cellPosition - cell);
    const auto value = [&] (std::uint32_t c, std::uint32_t salt) { return static_cast<float> ((hash (seed ^ c * 7919u ^ salt) & 0xffffu) / 32767.5 - 1.0); };
    const auto x = value (cell, 17u) + (value (cell + 1, 17u) - value (cell, 17u)) * fraction;
    const auto y = value (cell, 53u) + (value (cell + 1, 53u) - value (cell, 53u)) * fraction;
    const auto amount = std::clamp (randomness, 0.0f, 1.0f);
    return { x * (0.35f + amount * 0.65f), y * (0.35f + amount * 0.65f) };
}
std::pair<float, float> PathEngine::constantPowerPan (float x)
{
    const auto angle = (std::clamp (x, -1.0f, 1.0f) + 1.0f) * static_cast<float> (std::numbers::pi) * 0.25f;
    return { std::cos (angle), std::sin (angle) };
}
}
