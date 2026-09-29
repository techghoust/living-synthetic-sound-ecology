#pragma once

#include <cstdint>

namespace texture
{
class DeterministicRng final
{
public:
    void reset (const std::uint32_t seed) noexcept
    {
        state = seed != 0 ? seed : 0x54455831u;
    }

    [[nodiscard]] std::uint32_t next() noexcept
    {
        auto value = state;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        state = value;
        return value;
    }

    [[nodiscard]] float nextUnit() noexcept
    {
        return static_cast<float> (next()) / 4294967296.0f;
    }

private:
    std::uint32_t state = 0x54455831u;
};
} 