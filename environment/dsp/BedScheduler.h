#pragma once
#include <cstdint>

namespace lsse::environment
{
class BedScheduler
{
public:
    struct Layers { float bed{}, weather{}, events{}; };
    void prepare (double sampleRate, std::uint32_t seed);
    void reset();
    float sampleAt (std::uint64_t absoluteSample, bool generator, float density);
    Layers layersAt (std::uint64_t absoluteSample, bool generator, float density,
                     float variation, float scale) const;
private:
    static std::uint32_t hash (std::uint64_t key);
    double rate = 48000.0;
    std::uint32_t seedValue = 1;
};
}
