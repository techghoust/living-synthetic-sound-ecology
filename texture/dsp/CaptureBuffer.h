#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace texture
{
class CaptureBuffer final
{
public:
    using FramePosition = std::uint64_t;

    void prepare (std::size_t channels, std::size_t capacityInFrames);
    void clear() noexcept;
    void writeFrame (const float* input, std::size_t channelCount) noexcept;

    [[nodiscard]] bool readLinear (std::size_t channel, double absoluteFrame,
                                   float& result) const noexcept;
    [[nodiscard]] std::size_t getChannelCount() const noexcept { return storage.size(); }
    [[nodiscard]] std::size_t getCapacity() const noexcept { return capacity; }
    [[nodiscard]] std::size_t getValidFrameCount() const noexcept { return validFrames; }
    [[nodiscard]] FramePosition getTotalFramesWritten() const noexcept { return totalFramesWritten; }
    [[nodiscard]] FramePosition getOldestAvailableFrame() const noexcept;

private:
    std::vector<std::vector<float>> storage;
    std::size_t capacity = 0;
    std::size_t validFrames = 0;
    FramePosition totalFramesWritten = 0;
};
} 