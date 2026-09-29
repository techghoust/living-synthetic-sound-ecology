#include "DetailWear.h"

#include <algorithm>
#include <cmath>

namespace texture
{
void DetailWear::prepare (const double newSampleRate, const std::uint32_t seed) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset (seed);
}

void DetailWear::reset (const std::uint32_t seed) noexcept
{
    detailRng.reset (seed ^ 0xD37A11u);
    wearRng.reset (seed ^ 0x5EA412u);
    envelope = {};
    previousInput = {};
    previousNoise = {};
    previousShaperInput = {};
    heldSample = {};
    dropoutGain = 1.0f;
    dropoutTarget = 1.0f;
    dropoutSamplesRemaining = 0;
    rateCounter = 0;
    scheduleDropout();
}

void DetailWear::setParameters (const float detailAmount, const float wearAmount,
                                const float dropoutAmount) noexcept
{
    detail = std::clamp (detailAmount, 0.0f, 1.0f);
    wear = std::clamp (wearAmount, 0.0f, 1.0f);
    dropout = std::clamp (dropoutAmount, 0.0f, 1.0f);
}

void DetailWear::processFrame (float* samples, const std::size_t channels) noexcept
{
    if (samples == nullptr)
        return;

    const auto activeChannels = std::min (channels, maximumChannels);
    const auto envelopeRelease = static_cast<float> (std::exp (-1.0 / (sampleRate * 0.08)));
    const auto rateDivision = 1u + static_cast<std::size_t> (std::round (wear * wear * 11.0f));
    const auto refreshHeld = rateCounter == 0;
    rateCounter = (rateCounter + 1) % rateDivision;

    if (samplesUntilDropout > 0)
        --samplesUntilDropout;
    else if (dropoutSamplesRemaining == 0 && dropout > 0.0f)
    {
        dropoutSamplesRemaining = std::max<std::size_t> (1,
            static_cast<std::size_t> (sampleRate * (0.008 + 0.055 * wearRng.nextUnit())));
        dropoutTarget = std::max (0.03f, 1.0f - dropout * (0.55f + 0.4f * wearRng.nextUnit()));
    }
    if (dropoutSamplesRemaining > 0)
    {
        --dropoutSamplesRemaining;
        if (dropoutSamplesRemaining == 0)
        {
            dropoutTarget = 1.0f;
            scheduleDropout();
        }
    }
    const auto dropoutSlew = 1.0f - static_cast<float> (std::exp (-1.0 / (sampleRate * 0.002)));
    dropoutGain += (dropoutTarget - dropoutGain) * dropoutSlew;

    for (std::size_t channel = 0; channel < activeChannels; ++channel)
    {
        auto sample = std::isfinite (samples[channel]) ? samples[channel] : 0.0f;
        const auto absolute = std::abs (sample);
        envelope[channel] = std::max (absolute, envelope[channel] * envelopeRelease);
        const auto highFrequency = std::abs (sample - previousInput[channel]);
        previousInput[channel] = sample;
        const auto centroidProxy = highFrequency / (absolute + highFrequency + 1.0e-6f);
        if (detail > 0.0f)
        {
            const auto white = detailRng.nextUnit() * 2.0f - 1.0f;
            const auto highPassedNoise = white - previousNoise[channel] * 0.92f;
            previousNoise[channel] = white;
            sample += highPassedNoise * detail * envelope[channel]
                      * (0.04f + 0.16f * centroidProxy);
        }

        auto crackle = 0.0f;
        if (wear > 0.0f)
        {
            const auto oversampledMidpoint = 0.5f * (previousShaperInput[channel] + sample);
            previousShaperInput[channel] = sample;
            const auto drive = 1.0f + wear * 5.0f;
            const auto normalization = std::tanh (drive);
            const auto midpoint = std::tanh (oversampledMidpoint * drive) / normalization;
            const auto endpoint = std::tanh (sample * drive) / normalization;
            sample += (0.5f * (midpoint + endpoint) - sample) * wear;

            if (refreshHeld)
                heldSample[channel] = sample;
            sample += (heldSample[channel] - sample) * wear * 0.85f;
            const auto bitDepth = 16.0f - wear * 10.0f;
            const auto levels = std::pow (2.0f, bitDepth - 1.0f);
            const auto quantized = std::round (sample * levels) / levels;
            sample += (quantized - sample) * wear;
            if (wearRng.nextUnit() < wear * 0.00012f)
                crackle = (wearRng.nextUnit() * 2.0f - 1.0f) * wear * 0.18f;
        }
        samples[channel] = (sample + crackle) * dropoutGain;
    }
}

void DetailWear::scheduleDropout() noexcept
{
    const auto density = 0.15 + static_cast<double> (dropout * dropout) * 7.0;
    const auto jitter = 0.55 + static_cast<double> (wearRng.nextUnit()) * 0.9;
    samplesUntilDropout = std::max<std::size_t> (1,
        static_cast<std::size_t> (sampleRate * jitter / density));
}
} 