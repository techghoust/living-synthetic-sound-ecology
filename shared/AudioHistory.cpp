#include "AudioHistory.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace memory
{
void AudioHistory::prepare (const std::size_t channels,
                            const std::size_t capacityInFrames)
{
    if (channels == 0 || capacityInFrames == 0)
    {
        storage.clear();
        capacity = 0;
        clear();
        return;
    }

    std::vector<std::vector<float>> replacement (
        channels, std::vector<float> (capacityInFrames, 0.0f));

    storage.swap (replacement);
    capacity = capacityInFrames;
    clear();
}

void AudioHistory::clear() noexcept
{
    for (auto& channel : storage)
        std::fill (channel.begin(), channel.end(), 0.0f);

    validFrames = 0;
    writePosition = 0;
    totalFramesWritten = 0;
}

void AudioHistory::write (const float* const* inputChannels,
                          const std::size_t inputChannelCount,
                          const std::size_t frameCount) noexcept
{
    if (capacity == 0 || storage.empty() || frameCount == 0)
        return;

    const auto retainedFrames = std::min (frameCount, capacity);
    const auto sourceOffset = frameCount - retainedFrames;
    const auto absoluteStart = totalFramesWritten + sourceOffset;

    std::size_t copied = 0;
    while (copied < retainedFrames)
    {
        const auto ringIndex = static_cast<std::size_t> ((absoluteStart + copied) % capacity);
        const auto contiguous = std::min (retainedFrames - copied, capacity - ringIndex);

        for (std::size_t channel = 0; channel < storage.size(); ++channel)
        {
            auto* destination = storage[channel].data() + ringIndex;
            if (inputChannels != nullptr && channel < inputChannelCount
                && inputChannels[channel] != nullptr)
            {
                std::memcpy (destination,
                             inputChannels[channel] + sourceOffset + copied,
                             contiguous * sizeof (float));
            }
            else
            {
                std::fill_n (destination, contiguous, 0.0f);
            }
        }

        copied += contiguous;
    }

    totalFramesWritten += frameCount;
    writePosition = static_cast<std::size_t> (totalFramesWritten % capacity);

    const auto freeFrames = capacity - validFrames;
    validFrames = frameCount >= freeFrames ? capacity : validFrames + frameCount;
}

bool AudioHistory::copyFrames (const FramePosition absoluteStartFrame,
                               float* const* outputChannels,
                               const std::size_t outputChannelCount,
                               const std::size_t frameCount) const noexcept
{
    if (frameCount == 0)
        return true;

    if (capacity == 0 || outputChannels == nullptr
        || absoluteStartFrame < getOldestAvailableFrame()
        || absoluteStartFrame > totalFramesWritten
        || frameCount > totalFramesWritten - absoluteStartFrame)
    {
        return false;
    }

    std::size_t copied = 0;
    while (copied < frameCount)
    {
        const auto ringIndex = static_cast<std::size_t> ((absoluteStartFrame + copied) % capacity);
        const auto contiguous = std::min (frameCount - copied, capacity - ringIndex);

        for (std::size_t channel = 0; channel < outputChannelCount; ++channel)
        {
            if (outputChannels[channel] == nullptr)
                continue;

            auto* destination = outputChannels[channel] + copied;
            if (channel < storage.size())
            {
                std::memcpy (destination,
                             storage[channel].data() + ringIndex,
                             contiguous * sizeof (float));
            }
            else
            {
                std::fill_n (destination, contiguous, 0.0f);
            }
        }

        copied += contiguous;
    }

    return true;
}

bool AudioHistory::mixIntoFrames (const FramePosition absoluteStartFrame,
                                  const float* const* inputChannels,
                                  const std::size_t inputChannelCount,
                                  const std::size_t frameCount,
                                  const float gainStart,
                                  const float gainEnd) noexcept
{
    if (frameCount == 0)
        return true;

    if (capacity == 0 || inputChannels == nullptr
        || absoluteStartFrame < getOldestAvailableFrame()
        || absoluteStartFrame > totalFramesWritten
        || frameCount > totalFramesWritten - absoluteStartFrame)
    {
        return false;
    }

    if (gainStart <= 0.0f && gainEnd <= 0.0f)
        return true;

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const auto interpolation = frameCount > 1
                                       ? static_cast<float> (frame) / static_cast<float> (frameCount - 1)
                                       : 1.0f;
        const auto gain = std::clamp (gainStart + interpolation * (gainEnd - gainStart),
                                      0.0f, 0.85f);
        const auto ringIndex = static_cast<std::size_t> ((absoluteStartFrame + frame) % capacity);

        for (std::size_t channel = 0; channel < storage.size(); ++channel)
        {
            if (channel >= inputChannelCount || inputChannels[channel] == nullptr)
                continue;

            const auto feedbackSample = inputChannels[channel][frame];
            if (! std::isfinite (feedbackSample))
                continue;

            const auto combined = storage[channel][ringIndex]
                                  + gain * std::tanh (feedbackSample);
            storage[channel][ringIndex] = std::clamp (combined, -1.0f, 1.0f);
        }
    }

    return true;
}

bool AudioHistory::readSample (const std::size_t channel,
                               const FramePosition absoluteFrame,
                               float& result) const noexcept
{
    if (channel >= storage.size() || capacity == 0
        || absoluteFrame < getOldestAvailableFrame()
        || absoluteFrame >= totalFramesWritten)
    {
        return false;
    }

    result = storage[channel][static_cast<std::size_t> (absoluteFrame % capacity)];
    return true;
}

AudioHistory::FramePosition AudioHistory::getOldestAvailableFrame() const noexcept
{
    return totalFramesWritten - validFrames;
}

std::size_t AudioHistory::capacityForDuration (const double sampleRate,
                                               const double seconds)
{
    if (! std::isfinite (sampleRate) || ! std::isfinite (seconds)
        || sampleRate <= 0.0 || seconds <= 0.0)
    {
        return 0;
    }

    const auto frames = std::ceil (sampleRate * seconds);
    if (frames > static_cast<double> (std::numeric_limits<std::size_t>::max()))
        throw std::overflow_error ("Audio history capacity exceeds size_t");

    return static_cast<std::size_t> (frames);
}
} 