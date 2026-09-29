#include "MechanismEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace machine
{
namespace
{
float coefficientForMilliseconds (const double sampleRate, const double milliseconds) noexcept
{
    return static_cast<float> (std::exp (-1.0 / (sampleRate * milliseconds * 0.001)));
}
}

void MechanismEngine::prepare (const double sampleRate)
{
    rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    inputAttack = coefficientForMilliseconds (rate, 2.0);
    inputRelease = coefficientForMilliseconds (rate, 90.0);
    motorDecay = coefficientForMilliseconds (rate, 180.0);
    modulationDecay = coefficientForMilliseconds (rate, 220.0);
    modulationPhaseDelta = static_cast<float> (2.0 * std::numbers::pi * 1.7 / rate);
    const auto delayCapacity = static_cast<std::size_t> (std::ceil (rate * 0.125)) + 1U;
    for (auto& buffer : feedbackBuffers)
        buffer.assign (delayCapacity, 0.0f);
    reset();
}

void MechanismEngine::reset() noexcept
{
    relayVoices = {};
    inputEnvelope = 0.0f;
    motorPhase = 0.0f;
    motorPhaseDelta = 0.0f;
    motorEnergy = 0.0f;
    modulationPhase = 0.0f;
    modulationEnvelope = 0.0f;
    stepValue = 0.5f;
    stepAccent = 0.0f;
    bodyCoefficient = 0.0f;
    bodyRadiusSquared = 0.0f;
    bodyState1 = {};
    bodyState2 = {};
    toneState = {};
    drivePrevious = {};
    frictionNoiseState = { 0x13579bdfU, 0x2468ace1U };
    for (auto& buffer : feedbackBuffers)
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    feedbackWritePosition = 0;
    feedbackDelaySamples = 1;
    coefficientUpdateCountdown = 0;
    cachedTonePole = 0.0f;
    cachedDriveGain = 1.0f;
    cachedDriveNormalisation = 1.0f;
}

float MechanismEngine::nextNoise (std::uint32_t& state) noexcept
{
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return static_cast<float> (state) * (2.0f / 4294967295.0f) - 1.0f;
}

void MechanismEngine::trigger (const ScheduledEvent& event, const float excitation,
                               const Settings& settings) noexcept
{
    const auto safeExcitation = std::clamp (excitation, 0.0f, 1.0f);
    const auto force = std::clamp (settings.force, 0.0f, 1.0f);
    const auto accent = std::clamp (event.accent, 0.0f, 1.0f);
    const auto strength = safeExcitation * (0.2f + 0.8f * force)
                          * (0.65f + 0.55f * accent);

    const auto keyA = static_cast<std::uint32_t> (event.randomKey);
    const auto keyB = static_cast<std::uint32_t> (event.randomKey >> 32U);
    const auto normalizedA = static_cast<float> (keyA) / 4294967295.0f;
    const auto normalizedB = static_cast<float> (keyB) / 4294967295.0f;

    stepValue = std::clamp (event.value, 0.0f, 1.0f);
    stepAccent = accent;
    modulationEnvelope = std::max (modulationEnvelope, strength);
    const auto tone = std::clamp (settings.tone, -1.0f, 1.0f);
    const auto body = std::clamp (settings.body, 0.0f, 1.0f);
    const auto bodyFrequency = std::clamp (180.0f + 1500.0f * stepValue
                                           + 1300.0f * (tone + 1.0f) * 0.5f,
                                           80.0f, static_cast<float> (rate * 0.42));
    const auto bodyRadius = 0.91f + 0.084f * body;
    bodyCoefficient = 2.0f * bodyRadius * std::cos (static_cast<float> (
        2.0 * std::numbers::pi * bodyFrequency / rate));
    bodyRadiusSquared = bodyRadius * bodyRadius;
    if (! feedbackBuffers[0].empty())
    {
        const auto delaySeconds = 0.008f + 0.042f * (0.25f + 0.75f * stepValue);
        feedbackDelaySamples = std::clamp<std::size_t> (
            static_cast<std::size_t> (std::round (rate * delaySeconds)), 1,
            feedbackBuffers[0].size() - 1U);
    }

    if (settings.mode != Mode::relay)
    {
        const auto frequency = 34.0f + 118.0f * std::clamp (event.value, 0.0f, 1.0f)
                               + 22.0f * normalizedA;
        motorPhaseDelta = static_cast<float> (2.0 * std::numbers::pi * frequency / rate);
        motorEnergy = std::max (motorEnergy, strength);
    }

    if (settings.mode == Mode::motor)
        return;

    auto* selected = &relayVoices.front();
    for (auto& voice : relayVoices)
    {
        if (! voice.active)
        {
            selected = &voice;
            break;
        }
        if (voice.envelope < selected->envelope)
            selected = &voice;
    }

    const auto frequency = 850.0f + 4300.0f * normalizedA
                           + 500.0f * static_cast<float> (event.ratchetIndex);
    const auto decayMs = 5.0 + 24.0 * (1.0 - static_cast<double> (force));
    selected->active = true;
    selected->phase = 0.0f;
    selected->phaseDelta = static_cast<float> (2.0 * std::numbers::pi * frequency / rate);
    selected->envelope = strength;
    selected->decay = coefficientForMilliseconds (rate, decayMs);
    selected->pan = normalizedB * 2.0f - 1.0f;
    selected->noiseState = keyA != 0 ? keyA : 1;
}

std::array<float, 2> MechanismEngine::processFrame (const float inputMagnitude,
                                                    const Settings& settings) noexcept
{
    const auto magnitude = std::isfinite (inputMagnitude)
                               ? std::clamp (std::abs (inputMagnitude), 0.0f, 4.0f) : 0.0f;
    const auto envelopeCoefficient = magnitude > inputEnvelope ? inputAttack : inputRelease;
    inputEnvelope = magnitude + envelopeCoefficient * (inputEnvelope - magnitude);

    std::array<float, 2> output {};
    if (settings.mode != Mode::relay)
    {
        motorEnergy = std::max (motorEnergy * motorDecay, inputEnvelope * 0.08f);
        modulationPhase += modulationPhaseDelta;
        if (modulationPhase >= static_cast<float> (2.0 * std::numbers::pi))
            modulationPhase -= static_cast<float> (2.0 * std::numbers::pi);
        const auto modulation = modulationEnvelope
                                * (0.45f * (stepValue - 0.5f)
                                   + 0.3f * (stepAccent - 0.5f)
                                   + 0.25f * std::sin (modulationPhase));
        const auto irregularity = std::clamp (settings.irregularity, 0.0f, 1.0f);
        motorPhase += motorPhaseDelta * (1.0f + 0.12f * irregularity * modulation);
        if (motorPhase >= static_cast<float> (2.0 * std::numbers::pi))
            motorPhase -= static_cast<float> (2.0 * std::numbers::pi);
        const auto shaped = (std::sin (motorPhase) + 0.28f * std::sin (motorPhase * 2.0f))
                            * motorEnergy * std::clamp (settings.motor, 0.0f, 1.0f) * 0.55f;
        const auto offset = std::clamp (settings.stereo, 0.0f, 2.0f) * 0.035f;
        output[0] += shaped;
        output[1] += (std::sin (motorPhase + offset)
                      + 0.28f * std::sin ((motorPhase + offset) * 2.0f))
                     * motorEnergy * std::clamp (settings.motor, 0.0f, 1.0f) * 0.55f;
    }

    if (settings.mode != Mode::motor)
    {
        const auto width = std::clamp (settings.stereo, 0.0f, 2.0f);
        for (auto& voice : relayVoices)
        {
            if (! voice.active)
                continue;
            const auto click = (0.82f * std::sin (voice.phase)
                                + 0.18f * nextNoise (voice.noiseState))
                               * voice.envelope * std::clamp (settings.relay, 0.0f, 1.0f);
            const auto pan = std::clamp (voice.pan * width, -1.0f, 1.0f);
            output[0] += click * std::sqrt (0.5f * (1.0f - pan));
            output[1] += click * std::sqrt (0.5f * (1.0f + pan));
            voice.phase += voice.phaseDelta;
            if (voice.phase >= static_cast<float> (2.0 * std::numbers::pi))
                voice.phase -= static_cast<float> (2.0 * std::numbers::pi);
            voice.envelope *= voice.decay;
            if (voice.envelope < 1.0e-5f)
                voice.active = false;
        }
    }

    modulationEnvelope *= modulationDecay;

    const auto friction = std::clamp (settings.friction, 0.0f, 1.0f);
    const auto body = std::clamp (settings.body, 0.0f, 1.0f);
    const auto feedback = std::clamp (settings.feedback, 0.0f, 0.92f);
    const auto drive = std::clamp (settings.drive, 0.0f, 1.0f);
    const auto tone = std::clamp (settings.tone, -1.0f, 1.0f);
    if (coefficientUpdateCountdown <= 0)
    {
        const auto cutoff = std::clamp (
            500.0f * std::pow (24.0f, (tone + 1.0f) * 0.5f),
            120.0f, static_cast<float> (rate * 0.44));
        cachedTonePole = static_cast<float> (std::exp (-2.0 * std::numbers::pi
                                                       * cutoff / rate));
        cachedDriveGain = 1.0f + 14.0f * drive * drive;
        cachedDriveNormalisation = 1.0f
            / std::max (0.2f, std::tanh (cachedDriveGain));
        coefficientUpdateCountdown = 15;
    }
    else
    {
        --coefficientUpdateCountdown;
    }
    const auto delaySize = feedbackBuffers[0].size();
    const auto readPosition = delaySize == 0 ? 0
        : (feedbackWritePosition + delaySize - feedbackDelaySamples) % delaySize;
    for (std::size_t channel = 0; channel < output.size(); ++channel)
    {
        const auto noise = nextNoise (frictionNoiseState[channel]);
        const auto frictionLevel = friction
                                   * (0.18f * inputEnvelope
                                      + 0.008f * modulationEnvelope);
        auto signal = output[channel] + noise * frictionLevel;
        if (delaySize != 0)
            signal += feedbackBuffers[channel][readPosition] * feedback;

        const auto resonated = signal + bodyCoefficient * bodyState1[channel]
                               - bodyRadiusSquared * bodyState2[channel];
        bodyState2[channel] = bodyState1[channel];
        bodyState1[channel] = std::clamp (resonated, -16.0f, 16.0f);
        signal = signal * (1.0f - 0.3f * body) + resonated * (0.018f * body);

        toneState[channel] = signal + cachedTonePole * (toneState[channel] - signal);
        signal = toneState[channel];
        const auto preDrive = signal;
        if (drive > 1.0e-5f)
        {
            const auto midpoint = 0.5f * (drivePrevious[channel] + preDrive);
            signal = 0.5f * (std::tanh (midpoint * cachedDriveGain)
                             + std::tanh (preDrive * cachedDriveGain))
                     * cachedDriveNormalisation;
        }
        drivePrevious[channel] = preDrive;
        signal = std::isfinite (signal) ? std::tanh (signal) : 0.0f;
        if (delaySize != 0)
            feedbackBuffers[channel][feedbackWritePosition] = signal;
        output[channel] = signal;
    }
    if (delaySize != 0)
        feedbackWritePosition = (feedbackWritePosition + 1U) % delaySize;

    for (auto& sample : output)
        sample = std::isfinite (sample) ? std::tanh (sample) : 0.0f;
    return output;
}

std::size_t MechanismEngine::getActiveRelayVoiceCount() const noexcept
{
    return static_cast<std::size_t> (std::count_if (
        relayVoices.begin(), relayVoices.end(), [] (const auto& voice) { return voice.active; }));
}
} 
