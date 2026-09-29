#pragma once

#include "CaptureBuffer.h"
#include "DeterministicRng.h"

#include <cstddef>
#include <cstdint>
#include <array>

namespace texture
{
class GrainEngine final
{
public:
    static constexpr std::size_t maxVoices = 32;

    struct Settings
    {
        double grainSizeSeconds = 0.045;
        double densityPerSecond = 8.0;
        double spraySeconds = 0.18;
        float pitchSemitones = 0.0f;
        float reverseProbability = 0.05f;
        float captureAmount = 0.6f;
        float motionRateHz = 0.2f;
        float motionDepth = 0.2f;
        float stereoWidth = 1.0f;
        std::uint32_t seed = 1977;
    };

    void prepare (double sampleRate, const Settings& settings) noexcept;
    void reset() noexcept;
    void updateSettings (const Settings& settings) noexcept;
    void processFrame (const CaptureBuffer&, float* output, std::size_t channels) noexcept;

    [[nodiscard]] bool isGrainActive() const noexcept { return getActiveVoiceCount() != 0; }
    [[nodiscard]] std::size_t getActiveVoiceCount() const noexcept;
    [[nodiscard]] std::uint64_t getStartedGrainCount() const noexcept { return startedGrainCount; }
    [[nodiscard]] CaptureBuffer::FramePosition getLastSourceStart() const noexcept
    {
        return lastSourceStart;
    }

private:
    struct Grain
    {
        CaptureBuffer::FramePosition sourceStart = 0;
        std::size_t length = 0;
        double position = 0.0;
        double rate = 1.0;
        float gain = 0.0f;
        float leftGain = 1.0f;
        float rightGain = 1.0f;
        bool reverse = false;
        bool active = false;
        std::uint64_t serial = 0;
    };

    [[nodiscard]] std::size_t secondsToFrames (double seconds) const noexcept;
    [[nodiscard]] std::size_t intervalFrames() const noexcept;
    void startGrain (const CaptureBuffer&) noexcept;

    std::array<Grain, maxVoices> grains {};
    Settings settings;
    DeterministicRng rng;
    double sampleRate = 0.0;
    std::size_t samplesUntilNextGrain = 1;
    CaptureBuffer::FramePosition lastSourceStart = 0;
    std::uint64_t startedGrainCount = 0;
    double motionPhase = 0.0;
};
} 