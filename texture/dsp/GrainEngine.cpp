#include "GrainEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace texture
{
void GrainEngine::prepare (const double newSampleRate, const Settings& newSettings) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0 ? newSampleRate : 0.0;
    settings = newSettings;
    reset();
}

void GrainEngine::reset() noexcept
{
    grains = {};
    rng.reset (settings.seed);
    samplesUntilNextGrain = intervalFrames();
    lastSourceStart = 0;
    startedGrainCount = 0;
    motionPhase = 0.0;
}

void GrainEngine::updateSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    samplesUntilNextGrain = std::min (samplesUntilNextGrain, intervalFrames());
}

void GrainEngine::processFrame (const CaptureBuffer& capture, float* output,
                                const std::size_t channels) noexcept
{
    if (output == nullptr)
        return;

    std::fill_n (output, channels, 0.0f);

    if (samplesUntilNextGrain == 0)
    {
        startGrain (capture);
        samplesUntilNextGrain = intervalFrames();
    }

    constexpr auto twoPi = 6.28318530717958647692;
    for (auto& grain : grains)
    {
        if (! grain.active)
            continue;

        const auto denominator = static_cast<double> (std::max<std::size_t> (1, grain.length - 1));
        const auto envelope = static_cast<float> (
            0.5 - 0.5 * std::cos (twoPi * grain.position / denominator));
        const auto directed = grain.reverse
                                  ? static_cast<double> (grain.length - 1) - grain.position
                                  : grain.position;
        const auto absoluteFrame = static_cast<double> (grain.sourceStart) + directed;

        for (std::size_t channel = 0; channel < channels; ++channel)
        {
            float sample = 0.0f;
            if (capture.readLinear (channel, absoluteFrame, sample) && std::isfinite (sample))
            {
                auto channelGain = 1.0f;
                if (channels > 1)
                    channelGain = channel == 0 ? grain.leftGain : grain.rightGain;
                output[channel] += sample * envelope * grain.gain * channelGain;
            }
        }

        grain.position += grain.rate;
        if (grain.position >= static_cast<double> (grain.length - 1))
            grain.active = false;
    }

    if (samplesUntilNextGrain > 0)
        --samplesUntilNextGrain;

    const auto motionRate = std::clamp (settings.motionRateHz, 0.02f, 8.0f);
    if (sampleRate > 0.0)
    {
        motionPhase += twoPi * static_cast<double> (motionRate) / sampleRate;
        if (motionPhase >= twoPi)
            motionPhase -= twoPi;
    }
}

std::size_t GrainEngine::getActiveVoiceCount() const noexcept
{
    return static_cast<std::size_t> (std::count_if (grains.begin(), grains.end(),
        [] (const Grain& grain) { return grain.active; }));
}

std::size_t GrainEngine::secondsToFrames (const double seconds) const noexcept
{
    if (sampleRate <= 0.0 || ! std::isfinite (seconds) || seconds <= 0.0)
        return 0;
    const auto frames = std::round (seconds * sampleRate);
    return frames >= static_cast<double> (std::numeric_limits<std::size_t>::max())
               ? std::numeric_limits<std::size_t>::max()
               : static_cast<std::size_t> (frames);
}

std::size_t GrainEngine::intervalFrames() const noexcept
{
    const auto density = std::clamp (settings.densityPerSecond, 0.5, 80.0);
    return std::max<std::size_t> (1, secondsToFrames (1.0 / density));
}

void GrainEngine::startGrain (const CaptureBuffer& capture) noexcept
{
    const auto length = std::max<std::size_t> (2, secondsToFrames (
        std::clamp (settings.grainSizeSeconds, 0.005, 0.25)));
    const auto available = capture.getValidFrameCount();
    if (available <= length + 1 || settings.captureAmount <= 0.0f)
        return;

    auto* voice = static_cast<Grain*> (nullptr);
    for (auto& candidate : grains)
        if (! candidate.active)
        {
            voice = &candidate;
            break;
        }

    if (voice == nullptr)
        voice = &*std::min_element (grains.begin(), grains.end(),
            [] (const Grain& left, const Grain& right) { return left.serial < right.serial; });

    const auto depth = std::clamp (settings.motionDepth, 0.0f, 1.0f);
    const auto motion = static_cast<float> (std::sin (motionPhase));
    const auto sprayScale = std::clamp (1.0f + motion * depth * 0.75f, 0.25f, 1.75f);

    const auto maximumSpray = std::min (secondsToFrames (
        std::clamp (settings.spraySeconds * sprayScale, 0.0, 2.0)), available - length - 1);
    const auto offset = maximumSpray == 0 ? std::size_t { 0 }
                                          : std::min (maximumSpray,
                                              static_cast<std::size_t> (
                                                  rng.nextUnit() * (maximumSpray + 1)));
    const auto now = capture.getTotalFramesWritten();
    voice->sourceStart = now - length - 1 - offset;
    voice->length = length;
    voice->position = 0.0;
    const auto pitchScatter = (rng.nextUnit() * 2.0f - 1.0f) * depth * 12.0f;
    voice->rate = std::pow (2.0, static_cast<double> (
        std::clamp (settings.pitchSemitones + pitchScatter, -24.0f, 24.0f)) / 12.0);
    const auto expectedOverlap = std::max (1.0, settings.densityPerSecond
                                                  * settings.grainSizeSeconds);
    voice->gain = std::clamp (settings.captureAmount, 0.0f, 1.0f)
                  / static_cast<float> (std::sqrt (expectedOverlap));
    const auto pan = std::clamp ((rng.nextUnit() * 2.0f - 1.0f) * depth
                                     * std::clamp (settings.stereoWidth, 0.0f, 2.0f),
                                 -1.0f, 1.0f);
    constexpr auto halfPi = 1.57079632679489661923f;
    voice->leftGain = std::cos ((pan + 1.0f) * 0.5f * halfPi) * 1.41421356237f;
    voice->rightGain = std::sin ((pan + 1.0f) * 0.5f * halfPi) * 1.41421356237f;
    voice->reverse = rng.nextUnit() < std::clamp (settings.reverseProbability, 0.0f, 1.0f);
    voice->active = true;
    voice->serial = ++startedGrainCount;
    lastSourceStart = voice->sourceStart;
}
} 