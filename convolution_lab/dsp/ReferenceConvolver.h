#pragma once
#include <cstddef>
#include <vector>

namespace lsse::convolution
{
class ReferenceConvolver
{
public:
    void setImpulse (std::vector<float> impulse);
    void reset();
    float process (float input);
    static std::vector<float> resampleLinear (const std::vector<float>& source, double sourceRate, double targetRate);
    static std::vector<float> transformImpulse (const std::vector<float>& source, float start, float end,
                                                bool reverse, float stretch, float decay);
private:
    std::vector<float> ir { 1.0f }, history { 0.0f };
    std::size_t write = 0;
};
}
