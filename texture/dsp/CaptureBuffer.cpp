#include "CaptureBuffer.h"

#include <algorithm>
#include <cmath>

namespace texture
{
void CaptureBuffer::prepare (const std::size_t channels, const std::size_t capacityInFrames)
{
    storage.assign (channels, std::vector<float> (capacityInFrames, 0.0f));
    capacity = capacityInFrames;
    clear();
}

void CaptureBuffer::clear() noexcept
{
    for (auto& channel : storage)
        std::fill (channel.begin(), channel.end(), 0.0f);
    validFrames = 0;
    totalFramesWritten = 0;
}

void CaptureBuffer::writeFrame (const float* input, const std::size_t channelCount) noexcept
{
    if (capacity == 0 || storage.empty())
        return;

    const auto index = static_cast<std::size_t> (totalFramesWritten % capacity);
    for (std::size_t channel = 0; channel < storage.size(); ++channel)
    {
        const auto sample = input != nullptr && channel < channelCount ? input[channel] : 0.0f;
        storage[channel][index] = std::isfinite (sample) ? sample : 0.0f;
    }

    ++totalFramesWritten;
    validFrames = std::min (capacity, validFrames + 1);
}

bool CaptureBuffer::readLinear (const std::size_t channel, const double absoluteFrame,
                                float& result) const noexcept
{
    if (channel >= storage.size() || capacity == 0 || ! std::isfinite (absoluteFrame)
        || absoluteFrame < static_cast<double> (getOldestAvailableFrame())
        || absoluteFrame > static_cast<double> (totalFramesWritten - (validFrames > 0 ? 1 : 0))
        || validFrames == 0)
    {
        return false;
    }

    const auto first = static_cast<FramePosition> (std::floor (absoluteFrame));
    const auto second = std::min (first + 1, totalFramesWritten - 1);
    const auto fraction = static_cast<float> (absoluteFrame - static_cast<double> (first));
    const auto firstSample = storage[channel][static_cast<std::size_t> (first % capacity)];
    const auto secondSample = storage[channel][static_cast<std::size_t> (second % capacity)];
    result = firstSample + fraction * (secondSample - firstSample);
    return true;
}

CaptureBuffer::FramePosition CaptureBuffer::getOldestAvailableFrame() const noexcept
{
    return totalFramesWritten - validFrames;
}
} 