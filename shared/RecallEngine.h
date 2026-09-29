#pragma once

#include "AudioHistory.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace memory
{
struct MemoryEvent
{
    AudioHistory::FramePosition sourceStart = 0;
    std::size_t sourceLength = 0;
    double playbackPosition = 0.0;
    double playbackRate = 1.0;
    int direction = 1;
    float gain = 0.0f;
    std::size_t selectedAgeFrames = 0;
    std::size_t fadeLengthFrames = 0;
    float normalizedAge = 0.0f;
    float degradation = 0.0f;
    float corruption = 0.0f;
    float quantizationStep = 0.0f;
    float lowpassCoefficient = 1.0f;
    float driftDepth = 0.0f;
    double driftPhase = 0.0;
    double driftPhaseIncrement = 0.0;
    std::size_t repeatsRemaining = 0;
    std::array<float, 2> filterState {};
    bool active = false;
};

class RecallEngine final
{
public:
    static constexpr std::size_t maximumEvents = 4;

    struct Settings
    {
        bool enabled = true;
        double intervalSeconds = 3.0;
        double fragmentSeconds = 0.45;
        double minimumAgeSeconds = 0.75;
        double maximumAgeSeconds = 2.5;
        double memoryLengthSeconds = 30.0;
        float ageBias = 0.4f;
        float decayAmount = 0.0f;
        float corruptionAmount = 0.0f;
        float driftAmount = 0.0f;
        float repeatAmount = 0.0f;
        double fadeSeconds = 0.01;
        float gain = 0.35f;
        std::uint32_t randomSeed = 0x4d454d31u;
    };

    void prepare (double newSampleRate) noexcept;
    void prepare (double newSampleRate, const Settings& newSettings) noexcept;
    void reset() noexcept;
    void updateSettings (const Settings& newSettings) noexcept;

            [[nodiscard]] bool startEvent (const AudioHistory& history) noexcept;

        void processBlock (const AudioHistory& history,
                       float* const* outputChannels,
                       std::size_t outputChannelCount,
                       std::size_t frameCount) noexcept;

    void processBlock (const AudioHistory& history,
                       float* const* outputChannels,
                       std::size_t outputChannelCount,
                       std::size_t frameCount,
                       float wetGainStart,
                       float wetGainEnd) noexcept;

    [[nodiscard]] std::size_t getActiveEventCount() const noexcept;
    [[nodiscard]] const MemoryEvent& getLastStartedEvent() const noexcept
    {
        return lastStartedEvent;
    }

private:
    [[nodiscard]] std::size_t secondsToFrames (double seconds) const noexcept;
    [[nodiscard]] float nextRandomUnit() noexcept;
    [[nodiscard]] float eventEnvelope (const MemoryEvent& event) const noexcept;
    void finishOrRepeat (MemoryEvent& event) noexcept;
    void renderFrame (const AudioHistory& history,
                      float* const* outputChannels,
                      std::size_t outputChannelCount,
                      std::size_t outputFrame,
                      float wetGain) noexcept;

    std::array<MemoryEvent, maximumEvents> events {};
    MemoryEvent lastStartedEvent {};
    Settings settings {};
    double sampleRate = 0.0;
    std::size_t samplesUntilNextEvent = 0;
    std::uint32_t randomState = 0x4d454d31u;
};
} 