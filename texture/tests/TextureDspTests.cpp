#include "texture/dsp/CaptureBuffer.h"
#include "texture/dsp/DeterministicRng.h"
#include "texture/dsp/DetailWear.h"
#include "texture/dsp/GrainEngine.h"
#include "texture/dsp/SpectralSurface.h"

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

void requireNear (const float actual, const float expected, const float tolerance,
                  const std::string_view message)
{
    if (std::abs (actual - expected) > tolerance)
        throw std::runtime_error (std::string (message));
}

void testCaptureWrapAndInterpolation()
{
    texture::CaptureBuffer capture;
    capture.prepare (1, 4);
    for (int frame = 0; frame < 6; ++frame)
    {
        const auto value = static_cast<float> (frame);
        capture.writeFrame (&value, 1);
    }

    require (capture.getOldestAvailableFrame() == 2, "capture retained the wrong range");
    float result = 0.0f;
    require (capture.readLinear (0, 2.5, result), "fractional retained read failed");
    requireNear (result, 2.5f, 1.0e-6f, "fractional interpolation was incorrect");
    require (! capture.readLinear (0, 1.99, result), "overwritten capture remained readable");
    require (! capture.readLinear (1, 3.0, result), "missing channel was readable");

    const auto notFinite = std::numeric_limits<float>::quiet_NaN();
    capture.writeFrame (&notFinite, 1);
    require (capture.readLinear (0, 6.0, result), "sanitized frame was unavailable");
    requireNear (result, 0.0f, 0.0f, "non-finite input was not sanitized");
}

void testDeterministicRng()
{
    texture::DeterministicRng first;
    texture::DeterministicRng second;
    first.reset (1977);
    second.reset (1977);
    for (int index = 0; index < 100; ++index)
        require (first.next() == second.next(), "equal seeds produced different RNG streams");

    second.reset (1978);
    require (first.next() != second.next(), "different seeds produced the same continuation");
}

void testOneGrainIsWindowedAndDeterministic()
{
    texture::CaptureBuffer firstCapture;
    texture::CaptureBuffer secondCapture;
    firstCapture.prepare (1, 256);
    secondCapture.prepare (1, 256);

    texture::GrainEngine::Settings settings;
    settings.grainSizeSeconds = 0.01;
    settings.densityPerSecond = 80.0;
    settings.spraySeconds = 0.05;
    settings.captureAmount = 1.0f;
    settings.seed = 77;

    texture::GrainEngine first;
    texture::GrainEngine second;
    first.prepare (1000.0, settings);
    second.prepare (1000.0, settings);

    auto producedAudio = false;
    for (int frame = 0; frame < 100; ++frame)
    {
        const auto input = 0.25f + 0.001f * static_cast<float> (frame);
        firstCapture.writeFrame (&input, 1);
        secondCapture.writeFrame (&input, 1);
        float firstOutput = 0.0f;
        float secondOutput = 0.0f;
        first.processFrame (firstCapture, &firstOutput, 1);
        second.processFrame (secondCapture, &secondOutput, 1);
        requireNear (firstOutput, secondOutput, 0.0f,
                     "one-grain output was not deterministic");
        require (std::isfinite (firstOutput), "one-grain output was non-finite");
        producedAudio = producedAudio || std::abs (firstOutput) > 1.0e-6f;
    }

    require (producedAudio, "one-grain engine never produced audio");
    require (first.getLastSourceStart() == second.getLastSourceStart(),
             "deterministic grains selected different capture positions");
}

void testFixedVoicePoolAndStereoMotion()
{
    texture::CaptureBuffer capture;
    capture.prepare (2, 4096);

    texture::GrainEngine::Settings settings;
    settings.grainSizeSeconds = 0.25;
    settings.densityPerSecond = 80.0;
    settings.pitchSemitones = -24.0f;
    settings.captureAmount = 1.0f;
    settings.motionDepth = 0.0f;
    settings.seed = 99;

    texture::GrainEngine engine;
    engine.prepare (1000.0, settings);
    std::size_t maximumActive = 0;
    for (int frame = 0; frame < 1800; ++frame)
    {
        const std::array<float, 2> input { 0.25f, 0.25f };
        std::array<float, 2> output {};
        capture.writeFrame (input.data(), input.size());
        engine.processFrame (capture, output.data(), output.size());
        maximumActive = std::max (maximumActive, engine.getActiveVoiceCount());
        require (engine.getActiveVoiceCount() <= texture::GrainEngine::maxVoices,
                 "grain voice pool exceeded its fixed ceiling");
    }
    require (maximumActive == texture::GrainEngine::maxVoices,
             "high density did not exercise the complete grain pool");
    require (engine.getStartedGrainCount() > texture::GrainEngine::maxVoices,
             "deterministic voice stealing was not exercised");

    settings.grainSizeSeconds = 0.08;
    settings.pitchSemitones = 0.0f;
    settings.motionDepth = 1.0f;
    settings.stereoWidth = 2.0f;
    engine.prepare (1000.0, settings);
    capture.prepare (2, 4096);
    auto stereoDifference = 0.0f;
    for (int frame = 0; frame < 800; ++frame)
    {
        const std::array<float, 2> input { 0.4f, 0.4f };
        std::array<float, 2> output {};
        capture.writeFrame (input.data(), input.size());
        engine.processFrame (capture, output.data(), output.size());
        stereoDifference += std::abs (output[0] - output[1]);
        require (std::isfinite (output[0]) && std::isfinite (output[1]),
                 "motion produced non-finite stereo output");
    }
    require (stereoDifference > 0.01f, "Width/Motion produced no stereo scatter");
}

void testSpectralLatencyFreezeAndWearDeterminism()
{
    texture::SpectralSurface spectral;
    spectral.prepare (1);
    spectral.setParameters (0.0f, false, 0.0f);
    std::vector<float> rendered (1800, 0.0f);
    for (std::size_t frame = 0; frame < rendered.size(); ++frame)
    {
        const auto input = frame == 0 ? 1.0f : 0.0f;
        spectral.processFrame (&input, &rendered[frame], 1);
    }
    const auto peak = static_cast<std::size_t> (std::distance (rendered.begin(),
        std::max_element (rendered.begin(), rendered.end(), [] (const float left, const float right)
        {
            return std::abs (left) < std::abs (right);
        })));
    require (peak == static_cast<std::size_t> (texture::SpectralSurface::getLatencySamples()),
             "spectral impulse did not match the reported latency");
    require (std::abs (rendered[peak]) > 0.45f, "spectral identity lost excessive impulse level");

    spectral.reset();
    auto frozenEnergy = 0.0;
    for (int frame = 0; frame < 3200; ++frame)
    {
        spectral.setParameters (0.65f, frame >= 1000, 0.2f);
        const auto input = frame < 1000 ? static_cast<float> (0.3 * std::sin (frame * 0.071)) : 0.0f;
        float output = 0.0f;
        spectral.processFrame (&input, &output, 1);
        require (std::isfinite (output), "spectral freeze produced NaN/Inf");
        if (frame > 2200)
            frozenEnergy += std::abs (output);
    }
    require (frozenEnergy > 0.5, "spectral freeze did not sustain a captured surface");

    texture::DetailWear first;
    texture::DetailWear second;
    first.prepare (48000.0, 1234);
    second.prepare (48000.0, 1234);
    first.setParameters (0.8f, 0.7f, 0.9f);
    second.setParameters (0.8f, 0.7f, 0.9f);
    auto changedEnergy = 0.0;
    for (int frame = 0; frame < 12000; ++frame)
    {
        const auto input = static_cast<float> (0.25 * std::sin (frame * 0.017));
        float left = input;
        float right = input;
        first.processFrame (&left, 1);
        second.processFrame (&right, 1);
        requireNear (left, right, 0.0f, "Detail/Wear was not deterministic");
        require (std::isfinite (left), "Detail/Wear produced NaN/Inf");
        changedEnergy += std::abs (left - input);
    }
    require (changedEnergy > 0.1, "Detail/Wear did not affect the signal");
}
} 
int main()
{
    try
    {
        testCaptureWrapAndInterpolation();
        testDeterministicRng();
        testOneGrainIsWindowedAndDeterministic();
        testFixedVoicePoolAndStereoMotion();
        testSpectralLatencyFreezeAndWearDeterminism();
        std::cout << "All TEXTURE DSP tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TEXTURE DSP test failure: " << error.what() << '\n';
        return 1;
    }
}
