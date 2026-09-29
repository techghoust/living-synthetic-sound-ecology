#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace machine
{
constexpr std::size_t maxSteps = 16;
constexpr std::size_t maxEventsPerBlock = 256;

enum class TimingSource
{
    hostPpq,
    hostSamples,
    fallbackTempo,
    freeRun
};

enum class Discontinuity
{
    none,
    start,
    stop,
    seek,
    loopWrap,
    sourceChange
};

struct StepState
{
    float value = 1.0f;
    float accent = 0.0f;
    float probability = 1.0f;
    std::uint8_t ratchet = 1;
};

struct PatternSnapshot
{
    std::array<StepState, maxSteps> steps {};
    std::uint32_t revision = 1;
    std::uint8_t activeSteps = 16;
    float swing = 0.0f;
    std::uint32_t seed = 1977;
};

class PatternSnapshotQueue
{
public:
    bool push (const PatternSnapshot&) noexcept;
    bool pop (PatternSnapshot&) noexcept;

private:
    static constexpr std::size_t capacity = 8;
    std::array<PatternSnapshot, capacity> storage {};
    std::atomic<std::size_t> writePosition { 0 };
    std::atomic<std::size_t> readPosition { 0 };
};

struct HostPosition
{
    bool available = false;
    bool isPlaying = false;
    bool isLooping = false;
    std::optional<double> bpm;
    std::optional<double> ppq;
    std::optional<std::int64_t> timeInSamples;
    std::optional<double> loopStartPpq;
    std::optional<double> loopEndPpq;
};

struct BlockContext
{
    double sampleRate = 44100.0;
    int numSamples = 0;
    bool sync = true;
    std::uint8_t rateIndex = 2;
    HostPosition host;
};

struct ScheduledEvent
{
    int sampleOffset = 0;
    std::int64_t absoluteSample = 0;
    std::int64_t patternCycle = 0;
    std::uint8_t stepIndex = 0;
    std::uint8_t ratchetIndex = 0;
    std::uint64_t randomKey = 0;
    float accent = 0.0f;
    float value = 1.0f;

    bool operator== (const ScheduledEvent&) const = default;
};

struct EventBuffer
{
    std::array<ScheduledEvent, maxEventsPerBlock> events {};
    std::size_t size = 0;
    bool overflowed = false;
    TimingSource source = TimingSource::fallbackTempo;
    Discontinuity discontinuity = Discontinuity::none;
};

class TransportClock
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    EventBuffer processBlock (const BlockContext&, const PatternSnapshot&) noexcept;

    static double divisionPpq (std::uint8_t rateIndex) noexcept;
    static double freeRateHz (std::uint8_t rateIndex) noexcept;
    static std::uint64_t makeRandomKey (std::uint32_t seed, std::uint32_t revision,
                                        std::int64_t cycle, std::uint8_t step,
                                        std::uint8_t ratchet, std::uint8_t kind) noexcept;
    static double randomUnit (std::uint64_t key) noexcept;

private:
    EventBuffer schedulePpq (double startPpq, double bpm, std::int64_t blockStartSample,
                             int numSamples, std::uint8_t rateIndex,
                             const PatternSnapshot&, TimingSource, Discontinuity) noexcept;
    EventBuffer scheduleFree (const BlockContext&, const PatternSnapshot&,
                              Discontinuity) noexcept;
    Discontinuity classify (const BlockContext&, TimingSource, double startPpq) noexcept;

    double preparedSampleRate = 44100.0;
    std::uint64_t renderTime = 0;
    bool hadPrevious = false;
    bool previousPlaying = false;
    TimingSource previousSource = TimingSource::fallbackTempo;
    double previousStartPpq = 0.0;
    double previousBpm = 120.0;
    int previousNumSamples = 0;
    std::optional<std::int64_t> previousSamplePosition;
    std::optional<double> anchorPpq;
    std::optional<std::int64_t> anchorSample;
};

class TransportSimulator
{
public:
    struct TempoPoint
    {
        std::int64_t sample = 0;
        double bpm = 120.0;
        double ppq = 0.0;
    };

    void prepare (double sampleRate, double initialBpm = 120.0) noexcept;
    bool addTempoChange (std::int64_t sample, double bpm) noexcept;
    HostPosition positionAt (std::int64_t sample, bool playing = true) const noexcept;

private:
    static constexpr std::size_t maxTempoPoints = 32;
    std::array<TempoPoint, maxTempoPoints> points {};
    std::size_t pointCount = 0;
    double rate = 44100.0;
};
} 