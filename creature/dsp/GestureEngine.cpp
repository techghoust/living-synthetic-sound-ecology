#include "GestureEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace lsse::creature
{
void GestureEngine::prepare (double sampleRate, std::uint32_t seed) { rate = std::clamp (sampleRate, 8000.0, 384000.0); initialSeed = seed == 0 ? 1u : seed; reset(); }
void GestureEngine::reset() { envelope = phase = low = formantLow = formantBand = bodyState = chatterValue = 0.0; sampleCounter = 0; rng = initialSeed; }
float GestureEngine::process (float input, float breath, float growl, float follow,
                              float throat, float formant, float vowel, float chatter,
                              float body, float activity, float aggression, float variation,
                              float size, float tone, int behavior)
{
    const auto clean = std::isfinite (input) ? std::clamp<double> (input, -4.0, 4.0) : 0.0;
    envelope += (std::abs (clean) - envelope) * (clean == 0.0 ? 0.001 : 0.02);
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    const auto noise = static_cast<double> (static_cast<std::int32_t> (rng)) / 2147483648.0;
    const auto behaviorEnergy = std::array<double,4>{0.65,0.9,1.25,0.72}[std::clamp(behavior,0,3)];
    const auto varied = 1.0 + (noise * 0.08) * std::clamp<double> (variation, 0.0, 1.0);
    phase += 2.0 * std::numbers::pi * (30.0 + (1.0 - size) * 48.0 + envelope * 38.0) * varied / rate; if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
    const auto chatterPeriod = static_cast<std::uint64_t> (std::max (32.0, rate / (5.0 + activity * 35.0)));
    if ((sampleCounter++ % chatterPeriod) == 0) chatterValue = noise;
    const auto excitation = noise * breath * envelope + std::sin (phase) * growl * std::sqrt (envelope)
                          + chatterValue * chatter * envelope * (0.2 + aggression * 0.8);
    low += (excitation - low) * (0.03 + 0.15 * std::clamp<double> (follow, 0.0, 1.0));
    const auto voiced = std::tanh (low * (1.0 + throat * 8.0)) * behaviorEnergy;
    const auto centre = std::clamp (180.0 + vowel * 1450.0 + formant * 900.0 + tone * 600.0, 80.0, rate * 0.35);
    const auto f = 2.0 * std::sin (std::numbers::pi * centre / rate);
    formantLow += f * formantBand;
    const auto high = voiced - formantLow - (0.08 + 0.65 * (1.0 - formant)) * formantBand;
    formantBand += f * high;
    bodyState += (formantBand - bodyState) * (0.004 + 0.025 * (1.0 - size));
    return static_cast<float> (std::tanh (formantBand * 0.75 + bodyState * body * 1.8));
}
}
