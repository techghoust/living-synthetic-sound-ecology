#include "machine/dsp/MachineTiming.h"

#include <algorithm>
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

void requireNear (const double actual, const double expected, const double tolerance,
                  const std::string_view message)
{
    if (std::abs (actual - expected) > tolerance)
        throw std::runtime_error (std::string (message));
}

std::vector<machine::ScheduledEvent> render (const std::vector<int>& partitions)
{
    constexpr double sampleRate = 48000.0;
    constexpr std::int64_t totalSamples = 192000;
    machine::TransportSimulator simulator;
    simulator.prepare (sampleRate, 120.0);
    require (simulator.addTempoChange (96000, 90.0), "tempo change was rejected");
    machine::TransportClock clock;
    clock.prepare (sampleRate);
    machine::PatternSnapshot pattern;
    pattern.seed = 4321;
    pattern.revision = 7;
    pattern.steps[2].ratchet = 3;
    pattern.steps[5].probability = 0.4f;
    pattern.steps[9].accent = 0.8f;

    std::vector<machine::ScheduledEvent> events;
    std::int64_t position = 0;
    std::size_t partition = 0;
    while (position < totalSamples)
    {
        auto remaining = totalSamples - position;
        if (position < 96000)
            remaining = std::min<std::int64_t> (remaining, 96000 - position);
        const auto size = static_cast<int> (std::min<std::int64_t> (
            partitions[partition % partitions.size()], remaining));
        machine::BlockContext context;
        context.sampleRate = sampleRate;
        context.numSamples = size;
        context.sync = true;
        context.rateIndex = 2;
        context.host = simulator.positionAt (position);
        const auto block = clock.processBlock (context, pattern);
        require (! block.overflowed, "event buffer overflowed in partition test");
        events.insert (events.end(), block.events.begin(), block.events.begin()
                                                  + static_cast<std::ptrdiff_t> (block.size));
        position += size;
        ++partition;
    }
    return events;
}

void testRateAndSimulator()
{
    constexpr std::array<double, 7> expected { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0,
                                               0.5, 2.0 / 3.0, 1.0 };
    for (std::size_t index = 0; index < expected.size(); ++index)
        requireNear (machine::TransportClock::divisionPpq (static_cast<std::uint8_t> (index)),
                     expected[index], 1.0e-12, "rate division changed");

    machine::TransportSimulator simulator;
    simulator.prepare (48000.0, 120.0);
    requireNear (*simulator.positionAt (48000).ppq, 2.0, 1.0e-12,
                 "simulator PPQ conversion failed");
    require (simulator.addTempoChange (48000, 60.0), "valid tempo point was rejected");
    requireNear (*simulator.positionAt (96000).ppq, 3.0, 1.0e-12,
                 "simulator tempo segment was discontinuous");
}

void testPartitionIndependence()
{
    const auto regular = render ({ 256 });
    const auto irregular = render ({ 1, 7, 31, 64, 257, 13, 511 });
    require (regular.size() == irregular.size(),
             "event count changed with block partitioning");
    for (std::size_t index = 0; index < regular.size(); ++index)
    {
        const auto& a = regular[index];
        const auto& b = irregular[index];
        require (a.absoluteSample == b.absoluteSample
                 && a.patternCycle == b.patternCycle
                 && a.stepIndex == b.stepIndex
                 && a.ratchetIndex == b.ratchetIndex
                 && a.randomKey == b.randomKey
                 && a.accent == b.accent && a.value == b.value,
                 "event schedule changed with block partitioning");
    }
    require (! regular.empty(), "partition test scheduled no events");
}

void testSwingRatchetsAndProbability()
{
    machine::TransportClock clock;
    clock.prepare (48000.0);
    machine::PatternSnapshot pattern;
    pattern.swing = 1.0f;
    pattern.steps[0].ratchet = 4;
    machine::BlockContext context;
    context.sampleRate = 48000.0;
    context.numSamples = 15000;
    context.rateIndex = 2;
    context.host.available = true;
    context.host.isPlaying = true;
    context.host.bpm = 120.0;
    context.host.ppq = 0.0;
    context.host.timeInSamples = 0;
    const auto events = clock.processBlock (context, pattern);
    require (events.size >= 6, "swing/ratchet test scheduled too few events");
    require (events.events[0].sampleOffset == 0
             && events.events[1].sampleOffset == 2175
             && events.events[2].sampleOffset == 4350
             && events.events[3].sampleOffset == 6525,
             "ratchets did not divide the swung step");
    require (events.events[4].sampleOffset == 8700,
             "odd swing boundary was not delayed by 45 percent");

    clock.reset();
    pattern = {};
    for (auto& step : pattern.steps)
        step.probability = 0.0f;
    require (clock.processBlock (context, pattern).size == 0,
             "zero probability emitted an event");
}

void testTransportTransitionsAndFallbacks()
{
    machine::TransportClock clock;
    clock.prepare (48000.0);
    machine::PatternSnapshot pattern;
    machine::BlockContext context;
    context.sampleRate = 48000.0;
    context.numSamples = 6000;
    context.rateIndex = 2;
    context.host.available = true;
    context.host.isPlaying = false;
    context.host.bpm = 120.0;
    context.host.ppq = 0.0;
    context.host.timeInSamples = 0;
    require (clock.processBlock (context, pattern).size == 0,
             "stopped transport emitted events");

    context.host.isPlaying = true;
    context.host.ppq = 0.1;
    context.host.timeInSamples = 2400;
    const auto started = clock.processBlock (context, pattern);
    require (started.discontinuity == machine::Discontinuity::start,
             "transport start was not classified");
    require (started.size > 0 && started.events[0].stepIndex == 1,
             "mid-step start replayed the missed boundary");

    context.host.ppq = 8.0;
    context.host.timeInSamples = 192000;
    require (clock.processBlock (context, pattern).discontinuity == machine::Discontinuity::seek,
             "forward seek was not classified");

    clock.reset();
    context.host.ppq = 3.75;
    context.host.timeInSamples = 90000;
    context.host.isLooping = true;
    context.host.loopStartPpq = 0.0;
    context.host.loopEndPpq = 4.0;
    (void) clock.processBlock (context, pattern);
    context.host.ppq = 0.0;
    context.host.timeInSamples = 0;
    require (clock.processBlock (context, pattern).discontinuity
                 == machine::Discontinuity::loopWrap,
             "valid host loop was not classified");

    clock.reset();
    machine::BlockContext fallback;
    fallback.sampleRate = 48000.0;
    fallback.numSamples = 64;
    fallback.host.isPlaying = true;
    require (clock.processBlock (fallback, pattern).source
                 == machine::TimingSource::fallbackTempo,
             "missing timing did not use deterministic fallback");
}

void testSampleAnchorAndSnapshotQueue()
{
    machine::TransportClock clock;
    clock.prepare (48000.0);
    machine::PatternSnapshot pattern;
    machine::BlockContext context;
    context.sampleRate = 48000.0;
    context.numSamples = 64;
    context.host.isPlaying = true;
    context.host.bpm = 120.0;
    context.host.ppq = 10.0;
    context.host.timeInSamples = 240000;
    require (clock.processBlock (context, pattern).source == machine::TimingSource::hostPpq,
             "valid PPQ was not authoritative");
    context.host.ppq.reset();
    context.host.timeInSamples = 240064;
    require (clock.processBlock (context, pattern).source == machine::TimingSource::hostSamples,
             "sample-position fallback did not use the PPQ anchor");

    machine::PatternSnapshotQueue queue;
    for (std::uint32_t revision = 1; revision <= 7; ++revision)
    {
        pattern.revision = revision;
        require (queue.push (pattern), "snapshot queue rejected available capacity");
    }
    require (! queue.push (pattern), "snapshot queue did not report full capacity");
    for (std::uint32_t revision = 1; revision <= 7; ++revision)
    {
        machine::PatternSnapshot restored;
        require (queue.pop (restored) && restored.revision == revision,
                 "snapshot queue changed ordering");
    }
    machine::PatternSnapshot empty;
    require (! queue.pop (empty), "empty snapshot queue returned data");
}
} 
int main()
{
    try
    {
        testRateAndSimulator();
        testPartitionIndependence();
        testSwingRatchetsAndProbability();
        testTransportTransitionsAndFallbacks();
        testSampleAnchorAndSnapshotQueue();
        std::cout << "All MACHINE timing tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MACHINE timing test failure: " << error.what() << '\n';
        return 1;
    }
}
