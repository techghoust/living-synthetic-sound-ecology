#pragma once
#include <utility>
#include <cstdint>

namespace lsse::motion
{
class PathEngine
{
public:
    static std::pair<float, float> orbit (double phase);
    static std::pair<float, float> position (int path, double phase, std::uint32_t seed, float randomness);
    static std::pair<float, float> constantPowerPan (float x);
};
}
