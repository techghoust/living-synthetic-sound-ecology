#include "ModalBank.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lsse::material
{
void ModalBank::prepare (double sampleRate) { rate = std::clamp (sampleRate, 8000.0, 384000.0); reset(); }
void ModalBank::reset() { modes = {}; cavity.fill (0.0f); cavityWrite = 0; previousExcitation = 0.0; }
float ModalBank::process (float excitation, float size, float damping, float resonance,
                          float materialX, float materialY, float contact, float hardness,
                          float inharmonicity)
{
    static constexpr std::array<double, 8> softRatios { 1.0, 1.47, 2.10, 2.70, 3.86, 4.95, 6.22, 7.92 };
    static constexpr std::array<double, 8> metalRatios { 1.0, 1.62, 2.36, 3.15, 4.28, 5.58, 7.02, 8.74 };
    static constexpr std::array<double, 8> glassRatios { 1.0, 1.91, 2.83, 4.06, 5.42, 7.11, 8.92, 10.80 };
    const auto x = std::clamp<double> (materialX, 0.0, 1.0);
    const auto yProfile = std::clamp<double> (materialY, 0.0, 1.0);
    const auto hard = std::clamp<double> (hardness, 0.0, 1.0);
    const auto contactAmount = std::clamp<double> (contact, 0.0, 1.0);
    const auto inharmonic = std::clamp<double> (inharmonicity, 0.0, 1.0);
    const auto fundamental = 90.0 + 650.0 * (1.0 - std::clamp<double> (size, 0.0, 1.0));
    const auto tau = 0.025 + 1.8 * std::clamp<double> (resonance, 0.0, 1.0) * (1.0 - 0.85 * std::clamp<double> (damping, 0.0, 1.0));
    const auto radius = std::min (0.99995, std::exp (-1.0 / (tau * rate)));
    const auto clean = std::isfinite (excitation) ? static_cast<double> (excitation) : 0.0;
    const auto edge = clean - previousExcitation;
    previousExcitation = clean;
    const auto bounded = std::tanh ((clean + edge * contactAmount * (1.0 + 3.0 * hard)) * (1.2 + hard * 2.8));
    double sum = 0.0;
    for (std::size_t i = 0; i < modes.size(); ++i)
    {
        const auto xy = softRatios[i] + (metalRatios[i] - softRatios[i]) * x;
        const auto ratio = xy + (glassRatios[i] - xy) * yProfile;
        const auto stretched = ratio * (1.0 + inharmonic * 0.012 * static_cast<double> (i * i));
        const auto frequency = std::min (0.45 * rate, fundamental * stretched);
        const auto a1 = 2.0 * radius * std::cos (2.0 * std::numbers::pi * frequency / rate);
        const auto y = bounded * (0.10 / (1.0 + i * 0.35)) + a1 * modes[i].y1 - radius * radius * modes[i].y2;
        modes[i].y2 = modes[i].y1; modes[i].y1 = std::clamp (y, -8.0, 8.0);
        sum += modes[i].y1 / static_cast<double> (modes.size());
    }
    const auto delay = std::clamp<std::size_t> (static_cast<std::size_t> (24.0 + size * 1500.0), 1, cavity.size() - 1);
    const auto read = (cavityWrite + cavity.size() - delay) % cavity.size();
    const auto cavitySample = cavity[read];
    cavity[cavityWrite] = static_cast<float> (std::tanh (bounded * 0.12 + cavitySample * (0.12 + resonance * 0.42)));
    cavityWrite = (cavityWrite + 1) % cavity.size();
    return static_cast<float> (std::tanh (sum + cavitySample * (0.15 + 0.35 * yProfile)));
}
}
