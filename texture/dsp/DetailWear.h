#pragma once

#include "DeterministicRng.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace texture
{
class DetailWear final
{
public:
    static constexpr std::size_t maximumChannels = 2;

    void prepare (double sampleRate, std::uint32_t seed) noexcept;
    void reset (std::uint32_t seed) noexcept;
    void setParameters (float detailAmount, float wearAmount, float dropoutAmount) noexcept;
    void processFrame (float* samples, std::size_t channels) noexcept;

private:
    void scheduleDropout() noexcept;

    DeterministicRng detailRng;
    DeterministicRng wearRng;
    std::array<float, maximumChannels> envelope {};
    std::array<float, maximumChannels> previousInput {};
    std::array<float, maximumChannels> previousNoise {};
    std::array<float, maximumChannels> previousShaperInput {};
    std::array<float, maximumChannels> heldSample {};
    double sampleRate = 44100.0;
    float detail = 0.0f;
    float wear = 0.0f;
    float dropout = 0.0f;
    float dropoutGain = 1.0f;
    float dropoutTarget = 1.0f;
    std::size_t samplesUntilDropout = 1;
    std::size_t dropoutSamplesRemaining = 0;
    std::size_t rateCounter = 0;
};
} 