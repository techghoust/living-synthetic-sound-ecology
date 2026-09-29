#include "shared/AudioHistory.h"
#include "shared/RecallEngine.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
void require (const bool condition, const std::string_view message)
{
    if (! condition)
        throw std::runtime_error (std::string (message));
}

void requireEqual (const float actual, const float expected, const std::string_view message)
{
    if (std::abs (actual - expected) > 1.0e-6f)
        throw std::runtime_error (std::string (message));
}

memory::AudioHistory makeRampHistory (const std::size_t frameCount)
{
    memory::AudioHistory history;
    history.prepare (1, frameCount + 16);
    std::vector<float> ramp (frameCount);
    for (std::size_t i = 0; i < frameCount; ++i)
        ramp[i] = static_cast<float> (i);
    const float* input[] { ramp.data() };
    history.write (input, 1, ramp.size());
    return history;
}

memory::RecallEngine::Settings testSettings()
{
    memory::RecallEngine::Settings settings;
    settings.intervalSeconds = 100.0;
    settings.fragmentSeconds = 0.004;
    settings.minimumAgeSeconds = 0.002;
    settings.maximumAgeSeconds = 0.002;
    settings.fadeSeconds = 0.0;
    settings.gain = 0.5f;
    settings.randomSeed = 1234;
    return settings;
}

void testKnownFragmentRecall()
{
    auto history = makeRampHistory (10);
    memory::RecallEngine engine;
    engine.prepare (1000.0, testSettings());
    require (engine.startEvent (history), "known recall event did not start");

    std::array<float, 4> output { 1, 1, 1, 1 };
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size());

        for (std::size_t i = 0; i < output.size(); ++i)
        requireEqual (output[i], 1.0f + 0.5f * static_cast<float> (i + 4),
                      "recalled fragment was mixed incorrectly");

    require (engine.getActiveEventCount() == 0, "completed event remained active");
}

void testInsufficientHistory()
{
    auto history = makeRampHistory (5);
    auto settings = testSettings();
    settings.fragmentSeconds = 0.004;
    settings.minimumAgeSeconds = 0.002;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    require (! engine.startEvent (history), "event started without enough history");
}

void testEventPoolIsBounded()
{
    auto history = makeRampHistory (20);
    memory::RecallEngine engine;
    engine.prepare (1000.0, testSettings());

    for (std::size_t i = 0; i < memory::RecallEngine::maximumEvents; ++i)
        require (engine.startEvent (history), "fixed event slot was unavailable");

    require (! engine.startEvent (history), "event pool exceeded its fixed capacity");
    require (engine.getActiveEventCount() == memory::RecallEngine::maximumEvents,
             "active event count is incorrect");
}

void testDeterministicSelection()
{
    auto history = makeRampHistory (100);
    auto settings = testSettings();
    settings.fragmentSeconds = 0.01;
    settings.minimumAgeSeconds = 0.02;
    settings.maximumAgeSeconds = 0.04;

    memory::RecallEngine first;
    memory::RecallEngine second;
    first.prepare (1000.0, settings);
    second.prepare (1000.0, settings);
    require (first.startEvent (history), "first deterministic event failed");
    require (second.startEvent (history), "second deterministic event failed");
    require (first.getLastStartedEvent().sourceStart
                 == second.getLastStartedEvent().sourceStart,
             "same seed selected different history positions");
    require (first.getLastStartedEvent().selectedAgeFrames
                 == second.getLastStartedEvent().selectedAgeFrames,
             "same seed selected different ages");
}

void testAutomaticScheduling()
{
    auto history = makeRampHistory (10);
    auto settings = testSettings();
    settings.intervalSeconds = 0.002;
    settings.fragmentSeconds = 0.002;
    settings.minimumAgeSeconds = 0.002;
    settings.maximumAgeSeconds = 0.002;
    settings.gain = 1.0f;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    std::array<float, 4> output {};
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size());

    requireEqual (output[0], 0.0f, "scheduler fired too early");
    requireEqual (output[1], 0.0f, "scheduler fired too early");
    requireEqual (output[2], 6.0f, "scheduler did not recall expected first frame");
    requireEqual (output[3], 7.0f, "scheduler did not continue recalled event");
}

void testAgeBiasDistribution()
{
    auto history = makeRampHistory (1000);
    auto recentSettings = testSettings();
    recentSettings.fragmentSeconds = 0.01;
    recentSettings.minimumAgeSeconds = 0.01;
    recentSettings.maximumAgeSeconds = 0.8;
    recentSettings.memoryLengthSeconds = 1.0;
    recentSettings.ageBias = 0.0f;

    auto oldSettings = recentSettings;
    oldSettings.ageBias = 1.0f;

    memory::RecallEngine recent;
    memory::RecallEngine old;
    recent.prepare (1000.0, recentSettings);
    old.prepare (1000.0, oldSettings);
    require (recent.startEvent (history), "recent-biased event failed");
    require (old.startEvent (history), "old-biased event failed");
    require (recent.getLastStartedEvent().selectedAgeFrames
                 < old.getLastStartedEvent().selectedAgeFrames,
             "AGE did not bias selection from recent toward old history");
}

void testMemoryLengthWindow()
{
    auto history = makeRampHistory (1000);
    auto settings = testSettings();
    settings.fragmentSeconds = 0.01;
    settings.minimumAgeSeconds = 0.0;
    settings.maximumAgeSeconds = 1.0;
    settings.memoryLengthSeconds = 0.1;
    settings.ageBias = 1.0f;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    require (engine.startEvent (history), "window-limited event failed");
    require (engine.getLastStartedEvent().sourceStart >= 900,
             "event escaped configured MEMORY LENGTH window");
}

void testWetGainRamp()
{
    auto history = makeRampHistory (10);
    memory::RecallEngine engine;
    engine.prepare (1000.0, testSettings());
    require (engine.startEvent (history), "wet-ramp event failed");

    std::array<float, 4> output {};
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size(), 0.0f, 1.0f);
    requireEqual (output.front(), 0.0f, "wet ramp did not start at zero");
    requireEqual (output.back(), 3.5f, "wet ramp did not reach target gain");
}

void testRecallCanBeDisabled()
{
    auto history = makeRampHistory (20);
    auto settings = testSettings();
    settings.enabled = false;
    settings.intervalSeconds = 0.001;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    std::array<float, 8> output {};
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size());
    require (engine.getActiveEventCount() == 0, "disabled RECALL scheduled an event");
    for (const auto sample : output)
        requireEqual (sample, 0.0f, "disabled RECALL produced audio");
}

void testDecayIsAgeDependent()
{
    auto history = makeRampHistory (1000);
    auto recentSettings = testSettings();
    recentSettings.fragmentSeconds = 0.01;
    recentSettings.minimumAgeSeconds = 0.05;
    recentSettings.maximumAgeSeconds = 0.05;
    recentSettings.memoryLengthSeconds = 1.0;
    recentSettings.decayAmount = 1.0f;

    auto oldSettings = recentSettings;
    oldSettings.minimumAgeSeconds = 0.8;
    oldSettings.maximumAgeSeconds = 0.8;

    memory::RecallEngine recent;
    memory::RecallEngine old;
    recent.prepare (1000.0, recentSettings);
    old.prepare (1000.0, oldSettings);
    require (recent.startEvent (history), "recent decay event failed");
    require (old.startEvent (history), "old decay event failed");
    require (recent.getLastStartedEvent().degradation
                 < old.getLastStartedEvent().degradation,
             "older memory was not degraded more strongly");
}

void testCorruptionAndDriftEventState()
{
    auto history = makeRampHistory (100);
    auto settings = testSettings();
    settings.corruptionAmount = 1.0f;
    settings.driftAmount = 1.0f;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    require (engine.startEvent (history), "transformed event failed");
    const auto& event = engine.getLastStartedEvent();
    require (event.direction == -1, "full CORRUPTION did not reverse the event");
    require (event.quantizationStep > 0.0f, "CORRUPTION did not enable reconstruction loss");
    require (event.driftDepth > 0.0f, "DRIFT did not enable modulation");
    require (std::abs (event.playbackRate - 1.0) > 1.0e-6,
             "DRIFT did not vary base playback rate");
}

void testMaximumDegradationRemainsFinite()
{
    auto history = makeRampHistory (100);
    auto settings = testSettings();
    settings.decayAmount = 1.0f;
    settings.corruptionAmount = 1.0f;
    settings.driftAmount = 1.0f;
    settings.fadeSeconds = 0.001;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    require (engine.startEvent (history), "maximum degradation event failed");
    std::array<float, 16> output {};
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size());
    for (const auto sample : output)
        require (std::isfinite (sample), "degradation produced NaN/Inf");
}

void testRepeatCountAndDecay()
{
    auto history = makeRampHistory (20);
    auto settings = testSettings();
    settings.repeatAmount = 1.0f;

    memory::RecallEngine engine;
    engine.prepare (1000.0, settings);
    require (engine.startEvent (history), "repeating event failed to start");
    require (engine.getLastStartedEvent().repeatsRemaining == 3,
             "full REPEAT did not schedule three repeats");

    std::array<float, 16> output {};
    float* channels[] { output.data() };
    engine.processBlock (history, channels, 1, output.size());

    require (engine.getActiveEventCount() == 0, "repeating event did not finish");
    require (output[0] > output[4] && output[4] > output[8] && output[8] > output[12],
             "repeated fragments did not decay safely");
}
} 
int main()
{
    try
    {
        testKnownFragmentRecall();
        testInsufficientHistory();
        testEventPoolIsBounded();
        testDeterministicSelection();
        testAutomaticScheduling();
        testAgeBiasDistribution();
        testMemoryLengthWindow();
        testWetGainRamp();
        testRecallCanBeDisabled();
        testDecayIsAgeDependent();
        testCorruptionAndDriftEventState();
        testMaximumDegradationRemainsFinite();
        testRepeatCountAndDecay();
        std::cout << "All RecallEngine tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "RecallEngine test failure: " << error.what() << '\n';
        return 1;
    }
}
