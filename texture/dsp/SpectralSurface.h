#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstddef>

namespace texture
{
class SpectralSurface final
{
public:
    static constexpr int fftOrder = 9;
    static constexpr std::size_t fftSize = 1u << fftOrder;
    static constexpr std::size_t hopSize = fftSize / 4;
    static constexpr std::size_t maximumChannels = 2;

    void prepare (std::size_t channels) noexcept;
    void reset() noexcept;
    void setParameters (float blurAmount, bool freezeEnabled, float toneAmount) noexcept;
    void processFrame (const float* input, float* output, std::size_t channels) noexcept;

    [[nodiscard]] static constexpr int getLatencySamples() noexcept
    {
        return static_cast<int> (fftSize);
    }

private:
    using Frame = std::array<float, fftSize * 2>;
    using Ring = std::array<float, fftSize>;

    void renderSpectralFrame (std::size_t channel) noexcept;

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> window {};
    std::array<Ring, maximumChannels> inputRing {};
    std::array<Ring, maximumChannels> outputRing {};
    std::array<Frame, maximumChannels> fftFrames {};
    std::array<Frame, maximumChannels> frozenFrames {};
    std::array<std::array<float, fftSize / 2 + 1>, maximumChannels> previousMagnitudes {};
    std::size_t channelCount = 0;
    std::size_t ringPosition = 0;
    std::size_t samplesToTransform = hopSize;
    float blur = 0.0f;
    float tone = 0.0f;
    float freezeBlend = 0.0f;
    bool freezeRequested = false;
    bool freezeCaptured = false;
};
} 