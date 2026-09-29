#include "MachineTiming.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace machine
{
namespace
{
constexpr std::array<double, 7> divisions { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0,
                                             0.5, 2.0 / 3.0, 1.0 };
constexpr std::array<double, 7> freeRates { 16.0, 12.0, 8.0, 6.0, 4.0, 3.0, 2.0 };

bool validBpm (const std::optional<double>& value) noexcept
{
    return value && std::isfinite (*value) && *value >= 1.0 && *value <= 999.0;
}

bool validPpq (const std::optional<double>& value) noexcept
{
    return value && std::isfinite (*value);
}

std::int64_t floorDiv (const std::int64_t value, const std::int64_t divisor) noexcept
{
    const auto quotient = value / divisor;
    const auto remainder = value % divisor;
    return quotient - ((remainder != 0 && ((remainder < 0) != (divisor < 0))) ? 1 : 0);
}

std::int64_t positiveModulo (const std::int64_t value, const std::int64_t divisor) noexcept
{
    const auto remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

double boundaryFor (const std::int64_t globalStep, const double duration,
                    const double swing) noexcept
{
    const auto shift = positiveModulo (globalStep, 2) == 1
                           ? 0.45 * std::clamp (swing, 0.0, 1.0) * duration : 0.0;
    return static_cast<double> (globalStep) * duration + shift;
}

std::uint64_t mix64 (std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}
} 
bool PatternSnapshotQueue::push (const PatternSnapshot& snapshot) noexcept
{
    const auto write = writePosition.load (std::memory_order_relaxed);
    const auto next = (write + 1) % capacity;
    if (next == readPosition.load (std::memory_order_acquire))
        return false;
    storage[write] = snapshot;
    writePosition.store (next, std::memory_order_release);
    return true;
}

bool PatternSnapshotQueue::pop (PatternSnapshot& snapshot) noexcept
{
    const auto read = readPosition.load (std::memory_order_relaxed);
    if (read == writePosition.load (std::memory_order_acquire))
        return false;
    snapshot = storage[read];
    readPosition.store ((read + 1) % capacity, std::memory_order_release);
    return true;
}

void TransportClock::prepare (const double sampleRate) noexcept
{
    preparedSampleRate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void TransportClock::reset() noexcept
{
    renderTime = 0;
    hadPrevious = false;
    previousPlaying = false;
    previousSource = TimingSource::fallbackTempo;
    previousStartPpq = 0.0;
    previousBpm = 120.0;
    previousNumSamples = 0;
    previousSamplePosition.reset();
    anchorPpq.reset();
    anchorSample.reset();
}

double TransportClock::divisionPpq (const std::uint8_t index) noexcept
{
    return divisions[std::min<std::size_t> (index, divisions.size() - 1)];
}

double TransportClock::freeRateHz (const std::uint8_t index) noexcept
{
    return freeRates[std::min<std::size_t> (index, freeRates.size() - 1)];
}

std::uint64_t TransportClock::makeRandomKey (const std::uint32_t seed,
                                              const std::uint32_t revision,
                                              const std::int64_t cycle,
                                              const std::uint8_t step,
                                              const std::uint8_t ratchet,
                                              const std::uint8_t kind) noexcept
{
    auto key = mix64 (seed);
    key ^= mix64 (static_cast<std::uint64_t> (revision) << 1U);
    key ^= mix64 (static_cast<std::uint64_t> (cycle));
    key ^= mix64 ((static_cast<std::uint64_t> (step) << 16U)
                  | (static_cast<std::uint64_t> (ratchet) << 8U) | kind);
    return mix64 (key);
}

double TransportClock::randomUnit (const std::uint64_t key) noexcept
{
    return static_cast<double> (key >> 11U) * (1.0 / 9007199254740992.0);
}

Discontinuity TransportClock::classify (const BlockContext& context,
                                         const TimingSource source,
                                         const double startPpq) noexcept
{
    if (! hadPrevious)
        return context.host.isPlaying ? Discontinuity::start : Discontinuity::none;
    if (context.host.isPlaying != previousPlaying)
        return context.host.isPlaying ? Discontinuity::start : Discontinuity::stop;
    if (source != previousSource)
        return Discontinuity::sourceChange;
    if (! context.host.isPlaying)
        return Discontinuity::none;

    const auto expectedPpq = previousStartPpq
                             + previousNumSamples * previousBpm
                                   / (60.0 * preparedSampleRate);
    const auto tolerance = std::max (2.0 * previousBpm / (60.0 * preparedSampleRate), 1.0e-9);
    if (std::abs (startPpq - expectedPpq) <= tolerance)
        return Discontinuity::none;

    if (context.host.isLooping && context.host.loopStartPpq && context.host.loopEndPpq
        && *context.host.loopEndPpq > *context.host.loopStartPpq
        && std::abs (startPpq - *context.host.loopStartPpq) <= tolerance
        && std::abs (expectedPpq - *context.host.loopEndPpq) <= tolerance * 2.0)
        return Discontinuity::loopWrap;
    return Discontinuity::seek;
}

EventBuffer TransportClock::processBlock (const BlockContext& context,
                                           const PatternSnapshot& pattern) noexcept
{
    BlockContext safe = context;
    safe.sampleRate = std::isfinite (safe.sampleRate) && safe.sampleRate > 0.0
                          ? safe.sampleRate : preparedSampleRate;
    safe.numSamples = std::max (0, safe.numSamples);
    preparedSampleRate = safe.sampleRate;

    if (! safe.sync)
    {
        const auto discontinuity = hadPrevious && previousSource != TimingSource::freeRun
                                       ? Discontinuity::sourceChange : Discontinuity::none;
        auto result = scheduleFree (safe, pattern, discontinuity);
        renderTime += static_cast<std::uint64_t> (safe.numSamples);
        hadPrevious = true;
        previousPlaying = safe.host.isPlaying;
        previousSource = TimingSource::freeRun;
        previousNumSamples = safe.numSamples;
        return result;
    }

    const auto bpm = validBpm (safe.host.bpm) ? *safe.host.bpm : previousBpm;
    TimingSource source = TimingSource::fallbackTempo;
    auto startPpq = static_cast<double> (renderTime) * bpm / (60.0 * safe.sampleRate);
    auto blockStartSample = static_cast<std::int64_t> (renderTime);

    if (validPpq (safe.host.ppq) && validBpm (safe.host.bpm))
    {
        source = TimingSource::hostPpq;
        startPpq = *safe.host.ppq;
        if (safe.host.timeInSamples)
        {
            blockStartSample = *safe.host.timeInSamples;
            anchorPpq = startPpq;
            anchorSample = blockStartSample;
        }
    }
    else if (safe.host.timeInSamples && validBpm (safe.host.bpm)
             && anchorPpq && anchorSample)
    {
        source = TimingSource::hostSamples;
        blockStartSample = *safe.host.timeInSamples;
        startPpq = *anchorPpq + static_cast<double> (blockStartSample - *anchorSample)
                                  * bpm / (60.0 * safe.sampleRate);
    }

    const auto discontinuity = classify (safe, source, startPpq);
    EventBuffer result;
    if (safe.host.isPlaying)
        result = schedulePpq (startPpq, bpm, blockStartSample, safe.numSamples,
                              safe.rateIndex, pattern, source, discontinuity);
    else
    {
        result.source = source;
        result.discontinuity = discontinuity;
    }

    renderTime += static_cast<std::uint64_t> (safe.numSamples);
    hadPrevious = true;
    previousPlaying = safe.host.isPlaying;
    previousSource = source;
    previousStartPpq = startPpq;
    previousBpm = bpm;
    previousNumSamples = safe.numSamples;
    previousSamplePosition = safe.host.timeInSamples;
    return result;
}

EventBuffer TransportClock::schedulePpq (const double startPpq, const double bpm,
                                          const std::int64_t blockStartSample,
                                          const int numSamples, const std::uint8_t rateIndex,
                                          const PatternSnapshot& inputPattern,
                                          const TimingSource source,
                                          const Discontinuity discontinuity) noexcept
{
    EventBuffer result;
    result.source = source;
    result.discontinuity = discontinuity;
    if (numSamples <= 0 || ! std::isfinite (startPpq) || ! std::isfinite (bpm) || bpm <= 0.0)
        return result;

    const auto duration = divisionPpq (rateIndex);
    const auto ppqPerSample = bpm / (60.0 * preparedSampleRate);
    const auto endPpq = startPpq + static_cast<double> (numSamples) * ppqPerSample;
    const auto activeSteps = std::clamp<int> (inputPattern.activeSteps, 1, 16);
    const auto first = static_cast<std::int64_t> (std::floor (startPpq / duration)) - 2;
    const auto last = static_cast<std::int64_t> (std::ceil (endPpq / duration)) + 2;

    for (auto globalStep = first; globalStep <= last; ++globalStep)
    {
        const auto boundary = boundaryFor (globalStep, duration, inputPattern.swing);
        const auto nextBoundary = boundaryFor (globalStep + 1, duration, inputPattern.swing);
        const auto stepIndex = static_cast<std::uint8_t> (positiveModulo (globalStep, activeSteps));
        const auto cycle = floorDiv (globalStep, activeSteps);
        const auto& step = inputPattern.steps[stepIndex];
        const auto ratchets = std::clamp<int> (step.ratchet, 1, 4);
        for (int ratchet = 0; ratchet < ratchets; ++ratchet)
        {
            const auto eventPpq = boundary + (nextBoundary - boundary)
                                               * static_cast<double> (ratchet) / ratchets;
            const auto relative = (eventPpq - startPpq) / ppqPerSample;
            const auto offset = static_cast<int> (std::ceil (relative - 1.0e-9));
            if (offset < 0 || offset >= numSamples)
                continue;
            const auto key = makeRandomKey (inputPattern.seed, inputPattern.revision, cycle,
                                            stepIndex, static_cast<std::uint8_t> (ratchet), 0);
            const auto probability = std::clamp (step.probability, 0.0f, 1.0f);
            if (probability <= 0.0f
                || (probability < 1.0f && randomUnit (key) >= probability))
                continue;
            if (result.size == result.events.size())
            {
                result.overflowed = true;
                return result;
            }
            result.events[result.size++] = {
                offset, blockStartSample + offset, cycle, stepIndex,
                static_cast<std::uint8_t> (ratchet), key,
                std::clamp (step.accent, 0.0f, 1.0f), std::clamp (step.value, 0.0f, 1.0f)
            };
        }
    }
    return result;
}

EventBuffer TransportClock::scheduleFree (const BlockContext& context,
                                           const PatternSnapshot& pattern,
                                           const Discontinuity discontinuity) noexcept
{
    const auto bpmEquivalent = freeRateHz (context.rateIndex) * 60.0
                               * divisionPpq (context.rateIndex);
    return schedulePpq (static_cast<double> (renderTime) * bpmEquivalent
                            / (60.0 * preparedSampleRate),
                        bpmEquivalent, static_cast<std::int64_t> (renderTime),
                        context.numSamples, context.rateIndex, pattern,
                        TimingSource::freeRun, discontinuity);
}

void TransportSimulator::prepare (const double sampleRate, const double initialBpm) noexcept
{
    rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    pointCount = 1;
    points[0] = { 0, std::clamp (initialBpm, 1.0, 999.0), 0.0 };
}

bool TransportSimulator::addTempoChange (const std::int64_t sample, const double bpm) noexcept
{
    if (pointCount == 0 || pointCount == points.size() || sample <= points[pointCount - 1].sample
        || ! std::isfinite (bpm) || bpm < 1.0 || bpm > 999.0)
        return false;
    const auto& previous = points[pointCount - 1];
    const auto ppq = previous.ppq + static_cast<double> (sample - previous.sample)
                                       * previous.bpm / (60.0 * rate);
    points[pointCount++] = { sample, bpm, ppq };
    return true;
}

HostPosition TransportSimulator::positionAt (const std::int64_t sample,
                                              const bool playing) const noexcept
{
    HostPosition result;
    result.available = pointCount > 0;
    result.isPlaying = playing;
    if (pointCount == 0)
        return result;
    std::size_t index = 0;
    while (index + 1 < pointCount && points[index + 1].sample <= sample)
        ++index;
    const auto& point = points[index];
    result.bpm = point.bpm;
    result.ppq = point.ppq + static_cast<double> (sample - point.sample)
                               * point.bpm / (60.0 * rate);
    result.timeInSamples = sample;
    return result;
}
} 