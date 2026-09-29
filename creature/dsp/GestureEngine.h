#pragma once
#include <cstdint>

namespace lsse::creature
{
class GestureEngine
{
public:
    void prepare (double sampleRate, std::uint32_t seed);
    void reset();
    float process (float input, float breath, float growl, float follow,
                   float throat = 0.45f, float formant = 0.5f, float vowel = 0.5f,
                   float chatter = 0.1f, float body = 0.55f, float activity = 0.4f,
                   float aggression = 0.3f, float variation = 0.25f,
                   float size = 0.5f, float tone = 0.0f, int behavior = 0);
private:
    double rate = 48000.0, envelope = 0.0, phase = 0.0, low = 0.0;
    double formantLow = 0.0, formantBand = 0.0, bodyState = 0.0, chatterValue = 0.0;
    std::uint64_t sampleCounter = 0;
    std::uint32_t initialSeed = 1, rng = 1;
};
}
