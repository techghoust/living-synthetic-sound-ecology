#pragma once
#include <array>

namespace lsse::material
{
class ModalBank
{
public:
    void prepare (double sampleRate);
    void reset();
    float process (float excitation, float size, float damping, float resonance,
                   float materialX = 0.5f, float materialY = 0.5f,
                   float contact = 0.5f, float hardness = 0.5f,
                   float inharmonicity = 0.25f);
private:
    struct Mode { double y1{}, y2{}; };
    std::array<Mode, 8> modes {};
    std::array<float, 4096> cavity {};
    std::size_t cavityWrite = 0;
    double previousExcitation = 0.0;
    double rate = 48000.0;
};
}
