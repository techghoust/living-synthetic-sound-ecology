#include "shared/AudioHistory.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
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

void testCapacityCalculation()
{
    require (memory::AudioHistory::capacityForDuration (44100.0, 5.0) == 220500,
             "44.1 kHz duration conversion failed");
    require (memory::AudioHistory::capacityForDuration (48000.0, 30.0) == 1440000,
             "48 kHz duration conversion failed");
    require (memory::AudioHistory::capacityForDuration (192000.0, 120.0) == 23040000,
             "maximum planned duration conversion failed");
    require (memory::AudioHistory::capacityForDuration (0.0, 30.0) == 0,
             "invalid sample rate must produce zero capacity");
}

void testMinimumAndClear()
{
    memory::AudioHistory history;
    history.prepare (2, 1);

    const std::array<float, 1> left { 3.0f };
    const std::array<float, 1> right { 7.0f };
    const float* input[] { left.data(), right.data() };
    history.write (input, 2, 1);

    float result = 0.0f;
    require (history.readSample (0, 0, result), "single retained frame is unavailable");
    requireEqual (result, 3.0f, "single retained frame is incorrect");

    history.clear();
    require (history.getValidFrameCount() == 0, "clear did not reset valid frame count");
    require (! history.readSample (0, 0, result), "cleared frame remained readable");
}

void testStereoAndMissingChannelSilence()
{
    memory::AudioHistory history;
    history.prepare (2, 8);

    const std::array<float, 4> left { 1, 2, 3, 4 };
    const std::array<float, 4> right { 11, 12, 13, 14 };
    const float* stereo[] { left.data(), right.data() };
    history.write (stereo, 2, left.size());

    std::array<float, 4> leftOut {};
    std::array<float, 4> rightOut {};
    float* output[] { leftOut.data(), rightOut.data() };
    require (history.copyFrames (0, output, 2, 4), "stereo copy failed");

    for (std::size_t i = 0; i < left.size(); ++i)
    {
        requireEqual (leftOut[i], left[i], "left channel mismatch");
        requireEqual (rightOut[i], right[i], "right channel mismatch");
    }

    const std::array<float, 2> mono { 5, 6 };
    const float* monoInput[] { mono.data() };
    history.write (monoInput, 1, mono.size());
    require (history.copyFrames (4, output, 2, 2), "mono-to-stereo history copy failed");
    requireEqual (leftOut[0], 5.0f, "mono input was not recorded");
    requireEqual (rightOut[0], 0.0f, "missing input channel was not silenced");
    requireEqual (rightOut[1], 0.0f, "missing input channel was not silenced");
}

void testWraparoundAndCrossingRead()
{
    memory::AudioHistory history;
    history.prepare (1, 8);

    const std::array<float, 6> first { 0, 1, 2, 3, 4, 5 };
    const std::array<float, 4> second { 6, 7, 8, 9 };
    const float* firstInput[] { first.data() };
    const float* secondInput[] { second.data() };
    history.write (firstInput, 1, first.size());
    history.write (secondInput, 1, second.size());

    require (history.getWritePosition() == 2, "write position did not wrap correctly");
    require (history.getOldestAvailableFrame() == 2, "oldest frame is incorrect after wrap");

    std::array<float, 6> fragment {};
    float* output[] { fragment.data() };
    require (history.copyFrames (4, output, 1, fragment.size()),
             "fragment crossing ring boundary could not be read");

    for (std::size_t i = 0; i < fragment.size(); ++i)
        requireEqual (fragment[i], static_cast<float> (i + 4), "wrapped fragment mismatch");

    require (! history.copyFrames (1, output, 1, 1), "overwritten frame remained readable");
    require (! history.copyFrames (9, output, 1, 2), "future frame range was accepted");
}

void testOversizedWrite()
{
    memory::AudioHistory history;
    history.prepare (1, 4);

    const std::array<float, 6> inputData { 10, 11, 12, 13, 14, 15 };
    const float* input[] { inputData.data() };
    history.write (input, 1, inputData.size());

    require (history.getTotalFramesWritten() == 6, "oversized write broke absolute timeline");
    require (history.getOldestAvailableFrame() == 2, "oversized write retained wrong range");

    std::array<float, 4> outputData {};
    float* output[] { outputData.data() };
    require (history.copyFrames (2, output, 1, 4), "oversized write could not be read");
    for (std::size_t i = 0; i < outputData.size(); ++i)
        requireEqual (outputData[i], static_cast<float> (i + 12), "oversized write mismatch");
}

void testBoundedFeedbackMix()
{
    memory::AudioHistory history;
    history.prepare (1, 8);

    const std::array<float, 4> dry { 0.25f, -0.25f, 0.0f, 0.4f };
    const float* dryInput[] { dry.data() };
    history.write (dryInput, 1, dry.size());

    const std::array<float, 4> recalled { 0.5f, -0.5f, 100.0f,
                                          std::numeric_limits<float>::quiet_NaN() };
    const float* recalledInput[] { recalled.data() };
    require (history.mixIntoFrames (0, recalledInput, 1, recalled.size(), 0.5f, 0.5f),
             "valid feedback range was rejected");

    std::array<float, 4> result {};
    float* output[] { result.data() };
    require (history.copyFrames (0, output, 1, result.size()),
             "feedback result could not be read");
    requireEqual (result[0], 0.25f + 0.5f * std::tanh (0.5f),
                  "positive feedback mix was incorrect");
    requireEqual (result[1], -0.25f + 0.5f * std::tanh (-0.5f),
                  "negative feedback mix was incorrect");
    requireEqual (result[2], 0.5f, "feedback storage limiter did not clamp the result");
    requireEqual (result[3], 0.4f, "non-finite feedback changed history");
    require (! history.mixIntoFrames (5, recalledInput, 1, recalled.size(), 0.5f, 0.5f),
             "out-of-range feedback was accepted");
}

void testUnusualBlockSizesAndLongTimeline()
{
    memory::AudioHistory history;
    history.prepare (1, 257);

    constexpr std::array<std::size_t, 7> blockSizes { 1, 3, 17, 64, 2, 129, 31 };
    std::uint64_t frame = 0;
    for (int cycle = 0; cycle < 20; ++cycle)
    {
        for (const auto blockSize : blockSizes)
        {
            std::vector<float> block (blockSize);
            for (std::size_t i = 0; i < blockSize; ++i)
                block[i] = static_cast<float> (frame + i);

            const float* input[] { block.data() };
            history.write (input, 1, blockSize);
            frame += blockSize;
        }
    }

    require (history.getTotalFramesWritten() == frame, "timeline lost frames across block sizes");
    require (history.getValidFrameCount() == 257, "history did not reach full capacity");

    std::vector<float> retained (257);
    float* output[] { retained.data() };
    const auto oldest = history.getOldestAvailableFrame();
    require (history.copyFrames (oldest, output, 1, retained.size()),
             "long-running retained range could not be copied");

    for (std::size_t i = 0; i < retained.size(); ++i)
        requireEqual (retained[i], static_cast<float> (oldest + i),
                      "long-running history mismatch");
}
} 
int main()
{
    try
    {
        testCapacityCalculation();
        testMinimumAndClear();
        testStereoAndMissingChannelSilence();
        testWraparoundAndCrossingRead();
        testOversizedWrite();
        testBoundedFeedbackMix();
        testUnusualBlockSizesAndLongTimeline();
        std::cout << "All AudioHistory tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "AudioHistory test failure: " << error.what() << '\n';
        return 1;
    }
}
