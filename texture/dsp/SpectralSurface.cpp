#include "SpectralSurface.h"

#include <algorithm>
#include <cmath>

namespace texture
{
void SpectralSurface::prepare (const std::size_t channels) noexcept
{
    channelCount = std::clamp<std::size_t> (channels, 1, maximumChannels);
    constexpr auto twoPi = 6.28318530717958647692;
    for (std::size_t index = 0; index < fftSize; ++index)
        window[index] = static_cast<float> (0.5 - 0.5 * std::cos (
            twoPi * static_cast<double> (index) / static_cast<double> (fftSize - 1)));
    reset();
}

void SpectralSurface::reset() noexcept
{
    inputRing = {};
    outputRing = {};
    fftFrames = {};
    frozenFrames = {};
    previousMagnitudes = {};
    ringPosition = 0;
    samplesToTransform = hopSize;
    freezeBlend = 0.0f;
    freezeCaptured = false;
}

void SpectralSurface::setParameters (const float blurAmount, const bool freezeEnabled,
                                     const float toneAmount) noexcept
{
    blur = std::clamp (blurAmount, 0.0f, 1.0f);
    tone = std::clamp (toneAmount, -1.0f, 1.0f);
    freezeRequested = freezeEnabled;
}

void SpectralSurface::processFrame (const float* input, float* output,
                                    const std::size_t channels) noexcept
{
    if (input == nullptr || output == nullptr)
        return;

    const auto activeChannels = std::min ({ channels, channelCount, maximumChannels });
    for (std::size_t channel = 0; channel < activeChannels; ++channel)
    {
        output[channel] = outputRing[channel][ringPosition];
        outputRing[channel][ringPosition] = 0.0f;
        inputRing[channel][ringPosition] = std::isfinite (input[channel]) ? input[channel] : 0.0f;
    }
    for (std::size_t channel = activeChannels; channel < channels; ++channel)
        output[channel] = 0.0f;

    ringPosition = (ringPosition + 1) % fftSize;
    if (--samplesToTransform == 0)
    {
        if (freezeRequested && ! freezeCaptured)
        {
            for (std::size_t channel = 0; channel < activeChannels; ++channel)
            {
                auto& frame = fftFrames[channel];
                frame.fill (0.0f);
                for (std::size_t index = 0; index < fftSize; ++index)
                    frame[index] = inputRing[channel][(ringPosition + index) % fftSize]
                                   * window[index];
                fft.performRealOnlyForwardTransform (frame.data());
                frozenFrames[channel] = frame;
            }
            freezeCaptured = true;
        }

        const auto target = freezeRequested ? 1.0f : 0.0f;
        freezeBlend += std::clamp (target - freezeBlend, -0.25f, 0.25f);
        if (! freezeRequested && freezeBlend <= 0.0f)
            freezeCaptured = false;

        for (std::size_t channel = 0; channel < activeChannels; ++channel)
            renderSpectralFrame (channel);
        samplesToTransform = hopSize;
    }
}

void SpectralSurface::renderSpectralFrame (const std::size_t channel) noexcept
{
    auto& frame = fftFrames[channel];
    frame.fill (0.0f);
    for (std::size_t index = 0; index < fftSize; ++index)
        frame[index] = inputRing[channel][(ringPosition + index) % fftSize] * window[index];

    fft.performRealOnlyForwardTransform (frame.data());
    if (freezeCaptured && freezeBlend > 0.0f)
        for (std::size_t index = 0; index < fftSize * 2; ++index)
            frame[index] += (frozenFrames[channel][index] - frame[index]) * freezeBlend;

    auto magnitudes = previousMagnitudes[channel];
    const auto temporal = blur * blur * 0.97f;
    for (std::size_t bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto realIndex = bin * 2;
        const auto imaginaryIndex = realIndex + 1;
        const auto real = frame[realIndex];
        const auto imaginary = frame[imaginaryIndex];
        const auto magnitude = std::sqrt (real * real + imaginary * imaginary);
        const auto phase = std::atan2 (imaginary, real);
        const auto left = bin == 0 ? magnitude : magnitudes[bin - 1];
        const auto right = bin == fftSize / 2 ? magnitude : magnitudes[bin + 1];
        const auto spatial = (left + magnitude * 2.0f + right) * 0.25f;
        const auto blurred = magnitude + (spatial - magnitude) * blur;
        const auto smoothed = blurred + (previousMagnitudes[channel][bin] - blurred) * temporal;
        previousMagnitudes[channel][bin] = smoothed;
        const auto normalizedBin = static_cast<float> (bin) / static_cast<float> (fftSize / 2);
        const auto tilt = std::pow (2.0f, tone * (normalizedBin - 0.5f) * 2.0f);
        frame[realIndex] = std::cos (phase) * smoothed * tilt;
        frame[imaginaryIndex] = std::sin (phase) * smoothed * tilt;
    }

    fft.performRealOnlyInverseTransform (frame.data());
    constexpr auto overlapNormalization = 2.0f / 3.0f;
    for (std::size_t index = 0; index < fftSize; ++index)
    {
        const auto destination = (ringPosition + index) % fftSize;
        outputRing[channel][destination] += frame[index] * window[index]
                                             * overlapNormalization;
    }
}
} 