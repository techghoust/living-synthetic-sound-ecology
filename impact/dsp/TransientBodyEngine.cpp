#include "TransientBodyEngine.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lsse::impact
{
void TransientBodyEngine::prepare (double sampleRate) { rate = std::clamp (sampleRate, 8000.0, 384000.0); reset(); }
void TransientBodyEngine::reset() { fast = slow = phase = subPhase = envelope = contactEnvelope = debrisEnvelope = tailState = 0.0; hold = triggerCount = 0; armed = true; rng = 0x1234567u; }
float TransientBodyEngine::process (float input, float sensitivity, float bodyPitch, float decay,
                                    float pitchDrop, float attack, float attackTone,
                                    float sub, float debris, float tail, float tailSpace)
{
    const auto x = std::min (4.0, std::abs (std::isfinite (input) ? static_cast<double> (input) : 0.0));
    fast += (x - fast) * 0.22;
    slow += (x - slow) * 0.003;
    const auto threshold = 0.20 - 0.17 * std::clamp<double> (sensitivity, 0.0, 1.0);
    if (hold > 0) --hold;
    if (armed && hold == 0 && fast - slow > threshold)
    {
        envelope = std::min (1.0, 0.25 + x);
        contactEnvelope = envelope; debrisEnvelope = envelope;
        hold = static_cast<int> (rate * 0.012); armed = false; ++triggerCount;
    }
    if (fast - slow < threshold * 0.35) armed = true;
    const auto drop = std::clamp<double> (pitchDrop, 0.0, 1.0) * (1.0 - envelope) * 1.5;
    const auto frequency = 45.0 * std::pow (2.0, std::clamp<double> (bodyPitch, -1.0, 1.0) * 2.0 + drop);
    phase += 2.0 * std::numbers::pi * frequency / rate; if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
    subPhase += 2.0 * std::numbers::pi * frequency * 0.5 / rate; if (subPhase >= 2.0 * std::numbers::pi) subPhase -= 2.0 * std::numbers::pi;
    const auto seconds = 0.04 + 1.5 * std::clamp<double> (decay, 0.0, 1.0);
    envelope *= std::exp (-1.0 / (seconds * rate));
    contactEnvelope *= std::exp (-1.0 / ((0.002 + 0.035 * (1.0 - attack)) * rate));
    debrisEnvelope *= std::exp (-1.0 / ((0.015 + 0.18 * std::clamp<double> (tailSpace, 0.0, 1.0)) * rate));
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    const auto noise = static_cast<double> (static_cast<int> (rng)) / 2147483648.0;
    const auto contactLayer = std::copysign (std::sqrt (x), input) * contactEnvelope * (0.08 + 0.32 * attackTone);
    const auto bodyLayer = std::sin (phase) * envelope * 0.62;
    const auto subLayer = std::sin (subPhase) * envelope * std::clamp<double> (sub, 0.0, 1.0) * 0.34;
    const auto debrisLayer = noise * debrisEnvelope * std::clamp<double> (debris, 0.0, 1.0) * 0.24;
    tailState += ((bodyLayer + debrisLayer) - tailState) * (0.002 + 0.018 * (1.0 - tailSpace));
    const auto tailLayer = tailState * std::clamp<double> (tail, 0.0, 1.0) * 0.65;
    return static_cast<float> (std::tanh (contactLayer + bodyLayer + subLayer + debrisLayer + tailLayer));
}
}
