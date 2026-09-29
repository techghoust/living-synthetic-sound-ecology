#pragma once

namespace lsse::impact
{
class TransientBodyEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    float process (float input, float sensitivity, float bodyPitch, float decay,
                   float pitchDrop = 0.5f, float attack = 0.5f, float attackTone = 0.5f,
                   float sub = 0.4f, float debris = 0.3f, float tail = 0.3f,
                   float tailSpace = 0.5f);
    int getTriggerCount() const { return triggerCount; }
private:
    double rate = 48000.0, fast = 0.0, slow = 0.0, phase = 0.0, subPhase = 0.0;
    double envelope = 0.0, contactEnvelope = 0.0, debrisEnvelope = 0.0, tailState = 0.0;
    int hold = 0, triggerCount = 0;
    bool armed = true;
    unsigned int rng = 0x1234567u;
};
}
