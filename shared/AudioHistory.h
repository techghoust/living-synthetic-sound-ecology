#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace memory
{
class AudioHistory final
{
public:
    using FramePosition = std::uint64_t;

            void prepare (std::size_t channels, std::size_t capacityInFrames);
    void clear() noexcept;

                void write (const float* const* inputChannels,
                std::size_t inputChannelCount,
                std::size_t frameCount) noexcept;

                [[nodiscard]] bool copyFrames (FramePosition absoluteStartFrame,
                                   float* const* outputChannels,
                                   std::size_t outputChannelCount,
                                   std::size_t frameCount) const noexcept;

            [[nodiscard]] bool mixIntoFrames (FramePosition absoluteStartFrame,
                                      const float* const* inputChannels,
                                      std::size_t inputChannelCount,
                                      std::size_t frameCount,
                                      float gainStart,
                                      float gainEnd) noexcept;

    [[nodiscard]] bool readSample (std::size_t channel,
                                   FramePosition absoluteFrame,
                                   float& result) const noexcept;

    [[nodiscard]] std::size_t getChannelCount() const noexcept { return storage.size(); }
    [[nodiscard]] std::size_t getCapacityInFrames() const noexcept { return capacity; }
    [[nodiscard]] std::size_t getValidFrameCount() const noexcept { return validFrames; }
    [[nodiscard]] std::size_t getWritePosition() const noexcept { return writePosition; }
    [[nodiscard]] FramePosition getTotalFramesWritten() const noexcept { return totalFramesWritten; }
    [[nodiscard]] FramePosition getOldestAvailableFrame() const noexcept;

    [[nodiscard]] static std::size_t capacityForDuration (double sampleRate,
                                                          double seconds);

private:
    std::vector<std::vector<float>> storage;
    std::size_t capacity = 0;
    std::size_t validFrames = 0;
    std::size_t writePosition = 0;
    FramePosition totalFramesWritten = 0;
};
} 