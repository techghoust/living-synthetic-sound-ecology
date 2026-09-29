#pragma once

#include "machine/dsp/MachineTiming.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace machine
{
class MechanismEngine
{
public:
    static constexpr std::size_t maxRelayVoices = 16;

    enum class Mode : std::uint8_t
    {
        hybrid,
        motor,
        relay
    };

    struct Settings
    {
        Mode mode = Mode::hybrid;
        float force = 0.5f;
        float motor = 0.5f;
        float relay = 0.5f;
        float friction = 0.3f;
        float body = 0.5f;
        float feedback = 0.0f;
        float drive = 0.0f;
        float tone = 0.0f;
        float irregularity = 0.0f;
        float stereo = 1.0f;
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void trigger (const ScheduledEvent&, float excitation, const Settings&) noexcept;
    std::array<float, 2> processFrame (float inputMagnitude, const Settings&) noexcept;
    [[nodiscard]] std::size_t getActiveRelayVoiceCount() const noexcept;

private:
    struct RelayVoice
    {
        bool active = false;
        float phase = 0.0f;
        float phaseDelta = 0.0f;
        float envelope = 0.0f;
        float decay = 0.0f;
        float pan = 0.0f;
        std::uint32_t noiseState = 1;
    };

    static float nextNoise (std::uint32_t&) noexcept;

    std::array<RelayVoice, maxRelayVoices> relayVoices {};
    double rate = 44100.0;
    float inputEnvelope = 0.0f;
    float inputAttack = 0.0f;
    float inputRelease = 0.0f;
    float motorPhase = 0.0f;
    float motorPhaseDelta = 0.0f;
    float motorEnergy = 0.0f;
    float motorDecay = 0.0f;
    float modulationPhase = 0.0f;
    float modulationPhaseDelta = 0.0f;
    float modulationEnvelope = 0.0f;
    float modulationDecay = 0.0f;
    float stepValue = 0.5f;
    float stepAccent = 0.0f;
    float bodyCoefficient = 0.0f;
    float bodyRadiusSquared = 0.0f;
    std::array<float, 2> bodyState1 {};
    std::array<float, 2> bodyState2 {};
    std::array<float, 2> toneState {};
    std::array<float, 2> drivePrevious {};
    std::array<std::uint32_t, 2> frictionNoiseState { 0x13579bdfU, 0x2468ace1U };
    std::array<std::vector<float>, 2> feedbackBuffers;
    std::size_t feedbackWritePosition = 0;
    std::size_t feedbackDelaySamples = 1;
    int coefficientUpdateCountdown = 0;
    float cachedTonePole = 0.0f;
    float cachedDriveGain = 1.0f;
    float cachedDriveNormalisation = 1.0f;
};
} 
