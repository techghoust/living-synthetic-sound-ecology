#include "machine/dsp/MechanismEngine.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
void require (const bool condition, const std::string_view message)
{
    if (! condition)
        throw std::runtime_error (std::string (message));
}

double renderEnergy (machine::MechanismEngine& engine,
                     const machine::ScheduledEvent& event,
                     const machine::MechanismEngine::Settings& settings)
{
    engine.trigger (event, 0.8f, settings);
    auto energy = 0.0;
    for (int sample = 0; sample < 24000; ++sample)
    {
        const auto output = engine.processFrame (sample < 1000 ? 0.3f : 0.0f, settings);
        for (const auto value : output)
        {
            require (std::isfinite (value), "mechanism engine produced NaN/Inf");
            require (std::abs (value) <= 1.0f, "mechanism engine escaped its ceiling");
            energy += std::abs (value);
        }
    }
    return energy;
}

void testVoicesAndModes()
{
    machine::ScheduledEvent event;
    event.randomKey = 0x123456789abcdef0ULL;
    event.value = 0.72f;
    event.accent = 0.8f;
    machine::MechanismEngine::Settings settings;
    settings.force = 0.9f;
    settings.motor = 0.8f;
    settings.relay = 0.8f;
    settings.stereo = 1.4f;

    machine::MechanismEngine hybrid;
    hybrid.prepare (48000.0);
    settings.mode = machine::MechanismEngine::Mode::hybrid;
    require (renderEnergy (hybrid, event, settings) > 10.0,
             "hybrid mechanism produced no useful energy");

    machine::MechanismEngine motor;
    motor.prepare (48000.0);
    settings.mode = machine::MechanismEngine::Mode::motor;
    require (renderEnergy (motor, event, settings) > 5.0,
             "motor mode produced no useful energy");
    require (motor.getActiveRelayVoiceCount() == 0,
             "motor-only mode started relay voices");

    machine::MechanismEngine relay;
    relay.prepare (48000.0);
    settings.mode = machine::MechanismEngine::Mode::relay;
    require (renderEnergy (relay, event, settings) > 0.5,
             "relay mode produced no useful energy");
}

void testDeterminismAndVoiceLimit()
{
    machine::MechanismEngine first;
    machine::MechanismEngine second;
    first.prepare (44100.0);
    second.prepare (44100.0);
    machine::MechanismEngine::Settings settings;
    machine::ScheduledEvent event;
    event.value = 0.4f;
    event.accent = 0.3f;
    event.randomKey = 99887766;
    first.trigger (event, 0.7f, settings);
    second.trigger (event, 0.7f, settings);
    for (int sample = 0; sample < 8000; ++sample)
    {
        const auto a = first.processFrame (0.2f, settings);
        const auto b = second.processFrame (0.2f, settings);
        require (a == b, "same event key produced different mechanism audio");
    }

    for (std::size_t index = 0; index < 100; ++index)
    {
        event.randomKey = index + 1;
        first.trigger (event, 1.0f, settings);
    }
    require (first.getActiveRelayVoiceCount() <= machine::MechanismEngine::maxRelayVoices,
             "relay voice pool exceeded its fixed capacity");
}

void testPhaseFourModulesAndStress()
{
    machine::MechanismEngine engine;
    engine.prepare (96000.0);
    machine::ScheduledEvent event;
    event.randomKey = 0xfedcba9876543210ULL;
    event.value = 0.83f;
    event.accent = 1.0f;
    machine::MechanismEngine::Settings settings;
    settings.motor = 0.0f;
    settings.relay = 0.0f;
    settings.friction = 1.0f;
    settings.body = 0.95f;
    settings.feedback = 0.95f;
    settings.drive = 1.0f;
    settings.tone = 0.65f;
    settings.irregularity = 1.0f;
    engine.trigger (event, 1.0f, settings);

    auto energy = 0.0;
    for (int sample = 0; sample < 192000; ++sample)
    {
        const auto input = sample < 4000 ? 0.75f : 0.0f;
        const auto output = engine.processFrame (input, settings);
        for (const auto value : output)
        {
            require (std::isfinite (value), "phase-4 stress produced NaN/Inf");
            require (std::abs (value) <= 1.0f, "phase-4 stress escaped its ceiling");
            energy += std::abs (value);
        }
    }
    require (energy > 50.0, "friction/body/feedback/drive produced no useful energy");
}
} 
int main()
{
    try
    {
        testVoicesAndModes();
        testDeterminismAndVoiceLimit();
        testPhaseFourModulesAndStress();
        std::cout << "All MACHINE mechanism tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MACHINE mechanism test failure: " << error.what() << '\n';
        return 1;
    }
}
