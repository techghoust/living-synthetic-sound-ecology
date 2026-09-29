#include "RecallEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace memory
{
void RecallEngine::prepare (const double newSampleRate) noexcept
{
    prepare (newSampleRate, Settings {});
}

void RecallEngine::prepare (const double newSampleRate,
                            const Settings& newSettings) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0
                     ? newSampleRate
                     : 0.0;
    settings = newSettings;
    reset();
}

void RecallEngine::reset() noexcept
{
    events.fill (MemoryEvent {});
    lastStartedEvent = {};
    randomState = settings.randomSeed != 0 ? settings.randomSeed : 0x4d454d31u;
    samplesUntilNextEvent = secondsToFrames (settings.intervalSeconds);
}

void RecallEngine::updateSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    const auto intervalFrames = std::max<std::size_t> (1, secondsToFrames (settings.intervalSeconds));
    samplesUntilNextEvent = std::min (samplesUntilNextEvent, intervalFrames);
}

bool RecallEngine::startEvent (const AudioHistory& history) noexcept
{
    auto* freeEvent = static_cast<MemoryEvent*> (nullptr);
    for (auto& event : events)
    {
        if (! event.active)
        {
            freeEvent = &event;
            break;
        }
    }

    if (freeEvent == nullptr || sampleRate <= 0.0)
        return false;

    const auto fragmentFrames = std::max<std::size_t> (1, secondsToFrames (settings.fragmentSeconds));
    const auto memoryWindowFrames = secondsToFrames (settings.memoryLengthSeconds);
    const auto availableFrames = std::min (history.getValidFrameCount(), memoryWindowFrames);
    if (availableFrames <= fragmentFrames)
        return false;

    const auto maximumAvailableAge = availableFrames - fragmentFrames;
    const auto configuredMinimumAge = secondsToFrames (settings.minimumAgeSeconds);
    const auto configuredMaximumAge = secondsToFrames (settings.maximumAgeSeconds);
    if (configuredMinimumAge > maximumAvailableAge)
        return false;

    const auto maximumAge = std::min (std::max (configuredMinimumAge, configuredMaximumAge),
                                      maximumAvailableAge);
    const auto minimumAge = configuredMinimumAge;

    const auto ageRange = maximumAge - minimumAge;
    const auto uniform = nextRandomUnit();
    const auto ageBias = std::clamp (settings.ageBias, 0.0f, 1.0f);
    const auto exponent = 1.0f + 6.0f * std::abs (ageBias - 0.5f);
    const auto distributed = ageBias < 0.5f
                                 ? std::pow (uniform, exponent)
                                 : 1.0f - std::pow (1.0f - uniform, exponent);
    const auto selectedOffset = ageRange == 0
                                    ? std::size_t { 0 }
                                    : std::min (ageRange,
                                                static_cast<std::size_t> (
                                                    distributed * static_cast<float> (ageRange + 1)));
    const auto selectedAge = minimumAge + selectedOffset;
    const auto now = history.getTotalFramesWritten();
    const auto sourceStart = now - selectedAge - fragmentFrames;

    MemoryEvent event;
    event.sourceStart = sourceStart;
    event.sourceLength = fragmentFrames;
    event.playbackRate = 1.0;
    event.direction = 1;
    event.gain = std::clamp (settings.gain, 0.0f, 1.0f);
    event.selectedAgeFrames = selectedAge;
    event.normalizedAge = static_cast<float> (selectedAge)
                          / static_cast<float> (std::max<std::size_t> (1, memoryWindowFrames));
    event.degradation = std::clamp (
        settings.decayAmount * std::pow (event.normalizedAge, 0.75f), 0.0f, 1.0f);
    event.corruption = std::clamp (settings.corruptionAmount, 0.0f, 1.0f);
    event.direction = nextRandomUnit() < event.corruption ? -1 : 1;
    event.quantizationStep = event.corruption * event.corruption / 64.0f;

    const auto maximumCutoff = std::min (18000.0, sampleRate * 0.45);
    const auto cutoff = maximumCutoff * std::pow (0.05, static_cast<double> (event.degradation));
    event.lowpassCoefficient = static_cast<float> (
        1.0 - std::exp (-2.0 * 3.14159265358979323846 * cutoff / sampleRate));
    event.gain *= 1.0f - 0.3f * event.degradation;

    const auto drift = std::clamp (settings.driftAmount, 0.0f, 1.0f);
    event.playbackRate = 1.0 + 0.12 * static_cast<double> (drift)
                                   * static_cast<double> (nextRandomUnit() * 2.0f - 1.0f);
    event.driftDepth = 0.035f * drift;
    event.driftPhase = static_cast<double> (nextRandomUnit()) * 2.0 * 3.14159265358979323846;
    const auto driftFrequency = 0.15 + 0.85 * static_cast<double> (nextRandomUnit());
    event.driftPhaseIncrement = 2.0 * 3.14159265358979323846 * driftFrequency / sampleRate;
    const auto repeat = std::clamp (settings.repeatAmount, 0.0f, 1.0f);
    if (nextRandomUnit() < repeat)
        event.repeatsRemaining = 1 + static_cast<std::size_t> (std::floor (repeat * 2.0f));
    event.fadeLengthFrames = std::min (secondsToFrames (settings.fadeSeconds),
                                       fragmentFrames / 2);
    event.active = true;

    *freeEvent = event;
    lastStartedEvent = event;
    return true;
}

void RecallEngine::processBlock (const AudioHistory& history,
                                 float* const* outputChannels,
                                 const std::size_t outputChannelCount,
                                 const std::size_t frameCount) noexcept
{
    processBlock (history, outputChannels, outputChannelCount, frameCount, 1.0f, 1.0f);
}

void RecallEngine::processBlock (const AudioHistory& history,
                                 float* const* outputChannels,
                                 const std::size_t outputChannelCount,
                                 const std::size_t frameCount,
                                 const float wetGainStart,
                                 const float wetGainEnd) noexcept
{
    if (outputChannels == nullptr || outputChannelCount == 0 || frameCount == 0)
        return;

    const auto intervalFrames = std::max<std::size_t> (1, secondsToFrames (settings.intervalSeconds));

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        if (settings.enabled && samplesUntilNextEvent == 0)
        {
            (void) startEvent (history);
            samplesUntilNextEvent = intervalFrames;
        }

        const auto interpolation = frameCount > 1
                                       ? static_cast<float> (frame) / static_cast<float> (frameCount - 1)
                                       : 1.0f;
        const auto wetGain = wetGainStart + interpolation * (wetGainEnd - wetGainStart);
        renderFrame (history, outputChannels, outputChannelCount, frame, wetGain);

        if (settings.enabled && samplesUntilNextEvent > 0)
            --samplesUntilNextEvent;
    }
}

std::size_t RecallEngine::getActiveEventCount() const noexcept
{
    return static_cast<std::size_t> (std::count_if (
        events.begin(), events.end(), [] (const auto& event) { return event.active; }));
}

std::size_t RecallEngine::secondsToFrames (const double seconds) const noexcept
{
    if (sampleRate <= 0.0 || ! std::isfinite (seconds) || seconds <= 0.0)
        return 0;

    const auto frames = std::round (sampleRate * seconds);
    if (frames >= static_cast<double> (std::numeric_limits<std::size_t>::max()))
        return std::numeric_limits<std::size_t>::max();

    return static_cast<std::size_t> (frames);
}

float RecallEngine::nextRandomUnit() noexcept
{
    auto value = randomState;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    randomState = value;
    return static_cast<float> (value) / 4294967296.0f;
}

float RecallEngine::eventEnvelope (const MemoryEvent& event) const noexcept
{
    if (event.fadeLengthFrames == 0)
        return 1.0f;

    const auto fadeLength = static_cast<double> (event.fadeLengthFrames);
    const auto fadeIn = std::clamp (event.playbackPosition / fadeLength, 0.0, 1.0);
    const auto framesRemaining = static_cast<double> (event.sourceLength) - event.playbackPosition;
    const auto fadeOut = std::clamp (framesRemaining / fadeLength, 0.0, 1.0);
    return static_cast<float> (std::min (fadeIn, fadeOut));
}

void RecallEngine::finishOrRepeat (MemoryEvent& event) noexcept
{
    if (event.repeatsRemaining == 0)
    {
        event.active = false;
        return;
    }

    --event.repeatsRemaining;
    event.playbackPosition = 0.0;
    event.gain *= 0.82f;
    event.filterState.fill (0.0f);
}

void RecallEngine::renderFrame (const AudioHistory& history,
                                float* const* outputChannels,
                                const std::size_t outputChannelCount,
                                const std::size_t outputFrame,
                                const float wetGain) noexcept
{
    for (auto& event : events)
    {
        if (! event.active)
            continue;

        if (event.playbackPosition >= static_cast<double> (event.sourceLength))
        {
            finishOrRepeat (event);
            if (! event.active)
                continue;
        }

        const auto integerPosition = static_cast<std::size_t> (event.playbackPosition);
        const auto fraction = static_cast<float> (event.playbackPosition - integerPosition);
        const auto directedPosition = event.direction >= 0
                                          ? integerPosition
                                          : event.sourceLength - 1 - integerPosition;
        const auto nextDirectedPosition = event.direction >= 0
                                              ? std::min (directedPosition + 1, event.sourceLength - 1)
                                              : directedPosition > 0 ? directedPosition - 1 : 0;
        const auto firstFrame = event.sourceStart + directedPosition;
        const auto secondFrame = event.sourceStart + nextDirectedPosition;
        const auto eventGain = event.gain * eventEnvelope (event) * wetGain;

        for (std::size_t channel = 0; channel < outputChannelCount; ++channel)
        {
            if (outputChannels[channel] == nullptr)
                continue;

            float firstSample = 0.0f;
            float secondSample = 0.0f;
            if (history.readSample (channel, firstFrame, firstSample)
                && history.readSample (channel, secondFrame, secondSample))
            {
                const auto recalled = firstSample + fraction * (secondSample - firstSample);
                auto reconstructed = recalled;

                if (event.degradation > 0.0f && channel < event.filterState.size())
                {
                    auto& filter = event.filterState[channel];
                    filter += event.lowpassCoefficient * (recalled - filter);
                    reconstructed += event.degradation * (filter - reconstructed);
                }

                if (event.quantizationStep > 0.0f)
                    reconstructed = std::round (reconstructed / event.quantizationStep)
                                    * event.quantizationStep;

                outputChannels[channel][outputFrame] += eventGain * reconstructed;
            }
        }

        const auto driftModulation = 1.0 + static_cast<double> (event.driftDepth)
                                            * std::sin (event.driftPhase);
        const auto instantaneousRate = std::clamp (event.playbackRate * driftModulation, 0.25, 2.0);
        event.playbackPosition += instantaneousRate;
        event.driftPhase += event.driftPhaseIncrement;
        if (event.driftPhase >= 2.0 * 3.14159265358979323846)
            event.driftPhase -= 2.0 * 3.14159265358979323846;
        if (event.playbackPosition >= static_cast<double> (event.sourceLength))
            finishOrRepeat (event);
    }
}
} 