#include "texture/plugin/TextureProcessor.h"

#include <array>
#include <chrono>
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

juce::RangedAudioParameter& parameter (TextureAudioProcessor& processor,
                                       const juce::String& id)
{
    for (auto* candidate : processor.getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (candidate);
            identified != nullptr && identified->paramID == id)
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (candidate))
                return *ranged;
    throw std::runtime_error ("parameter not found: " + id.toStdString());
}

void setActualValue (juce::RangedAudioParameter& target, const float value)
{
    target.setValueNotifyingHost (target.convertTo0to1 (value));
}

float actualValue (const juce::RangedAudioParameter& target)
{
    return target.convertFrom0to1 (target.getValue());
}

void testParameterContractAndState()
{
    TextureAudioProcessor source;
    require (source.getParameters().size() == 19, "unexpected TEXTURE parameter count");
    setActualValue (parameter (source, "grain_size"), 88.0f);
    setActualValue (parameter (source, "density"), 23.0f);
    setActualValue (parameter (source, "seed"), 4321.0f);
    setActualValue (parameter (source, "motion_rate"), 3.2f);
    setActualValue (parameter (source, "motion_depth"), 76.0f);
    setActualValue (parameter (source, "width"), 145.0f);
    setActualValue (parameter (source, "blur"), 54.0f);
    setActualValue (parameter (source, "freeze"), 1.0f);
    setActualValue (parameter (source, "detail"), 61.0f);
    setActualValue (parameter (source, "wear"), 37.0f);
    setActualValue (parameter (source, "dropout"), 42.0f);
    setActualValue (parameter (source, "tone"), -28.0f);
    setActualValue (parameter (source, "mix"), 67.0f);
    setActualValue (parameter (source, "output"), -4.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    require (! state.isEmpty(), "TEXTURE state was empty");

    TextureAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    requireNear (actualValue (parameter (restored, "grain_size")), 88.0f, 0.11f,
                 "GRAIN SIZE was not restored");
    requireNear (actualValue (parameter (restored, "density")), 23.0f, 0.11f,
                 "DENSITY was not restored");
    requireNear (actualValue (parameter (restored, "seed")), 4321.0f, 0.1f,
                 "SEED was not restored");
    requireNear (actualValue (parameter (restored, "motion_rate")), 3.2f, 0.01f,
                 "MOTION RATE was not restored");
    requireNear (actualValue (parameter (restored, "motion_depth")), 76.0f, 0.11f,
                 "MOTION DEPTH was not restored");
    requireNear (actualValue (parameter (restored, "width")), 145.0f, 0.11f,
                 "WIDTH was not restored");
    requireNear (actualValue (parameter (restored, "blur")), 54.0f, 0.11f,
                 "BLUR was not restored");
    requireNear (actualValue (parameter (restored, "freeze")), 1.0f, 0.0f,
                 "FREEZE was not restored");
    requireNear (actualValue (parameter (restored, "detail")), 61.0f, 0.11f,
                 "DETAIL was not restored");
    requireNear (actualValue (parameter (restored, "wear")), 37.0f, 0.11f,
                 "WEAR was not restored");
    requireNear (actualValue (parameter (restored, "dropout")), 42.0f, 0.11f,
                 "DROPOUT was not restored");
    requireNear (actualValue (parameter (restored, "tone")), -28.0f, 0.11f,
                 "TONE was not restored");
    requireNear (actualValue (parameter (restored, "mix")), 67.0f, 0.11f,
                 "MIX was not restored");
    requireNear (actualValue (parameter (restored, "output")), -4.0f, 0.11f,
                 "OUTPUT was not restored");
}

void testFactoryPresetsAndProgramRecall()
{
    TextureAudioProcessor source;
    require (source.getNumPrograms() == 1, "host program contract changed");
    require (source.getNumFactoryPresets() == 8, "unexpected factory preset count");
    require (source.getFactoryPresetName (3) == "FROZEN GLASS", "factory preset name changed");
    source.applyFactoryPreset (4);
    require (source.getCurrentFactoryPreset() == 4, "factory preset was not selected");
    requireNear (actualValue (parameter (source, "wear")), 72.0f, 0.11f,
                 "BROKEN TAPE did not set WEAR");
    requireNear (actualValue (parameter (source, "mix")), 72.0f, 0.11f,
                 "BROKEN TAPE did not set MIX");

    juce::MemoryBlock state;
    source.getStateInformation (state);
    TextureAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    require (restored.getCurrentFactoryPreset() == 4, "selected factory preset was not restored");
    requireNear (actualValue (parameter (restored, "dropout")), 58.0f, 0.11f,
                 "factory preset parameters were not restored");
    restored.setCurrentProgram (0);
    require (restored.getCurrentFactoryPreset() == 4,
             "host Program 0 overwrote the restored factory preset");
    requireNear (actualValue (parameter (restored, "dropout")), 58.0f, 0.11f,
                 "host Program 0 overwrote restored parameters");
}

void testDryNullAndNonFiniteBoundary()
{
    TextureAudioProcessor processor;
    setActualValue (parameter (processor, "mix"), 0.0f);
    processor.prepareToPlay (48000.0, 257);
    juce::MidiBuffer midi;
    constexpr std::array<int, 6> sizes { 1, 17, 64, 129, 257, 31 };
    constexpr int totalSamples = 1400;
    std::vector<float> source (totalSamples);
    for (int sample = 0; sample < totalSamples; ++sample)
        source[static_cast<std::size_t> (sample)] = static_cast<float> (
            0.3 * std::sin (static_cast<double> (sample) * 0.031));

    int position = 0;
    std::size_t partition = 0;
    while (position < totalSamples)
    {
        const auto size = std::min (sizes[partition % sizes.size()], totalSamples - position);
        juce::AudioBuffer<float> buffer (2, size);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto value = source[static_cast<std::size_t> (position + sample)];
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto sourceIndex = position + sample
                                     - texture::SpectralSurface::getLatencySamples();
            const auto expected = sourceIndex >= 0
                                      ? source[static_cast<std::size_t> (sourceIndex)] : 0.0f;
            requireNear (buffer.getSample (0, sample), expected,
                         1.0e-6f, "latency-aligned 0% MIX changed dry audio");
            requireNear (buffer.getSample (1, sample), -expected,
                         1.0e-6f, "latency-aligned 0% MIX changed stereo dry audio");
        }
        position += size;
        ++partition;
    }

    juce::AudioBuffer<float> invalid (2, 1);
    invalid.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
    invalid.setSample (1, 0, std::numeric_limits<float>::infinity());
    processor.processBlock (invalid, midi);
    require (std::isfinite (invalid.getSample (0, 0)) && std::isfinite (invalid.getSample (1, 0)),
             "non-finite input escaped the processor boundary");
}

std::vector<float> renderWithPartitions (const std::vector<int>& partitions)
{
    TextureAudioProcessor processor;
    setActualValue (parameter (processor, "capture"), 100.0f);
    setActualValue (parameter (processor, "grain_size"), 20.0f);
    setActualValue (parameter (processor, "density"), 60.0f);
    setActualValue (parameter (processor, "spray"), 80.0f);
    setActualValue (parameter (processor, "reverse"), 40.0f);
    setActualValue (parameter (processor, "motion_rate"), 2.7f);
    setActualValue (parameter (processor, "motion_depth"), 85.0f);
    setActualValue (parameter (processor, "width"), 160.0f);
    setActualValue (parameter (processor, "seed"), 321.0f);
    setActualValue (parameter (processor, "mix"), 100.0f);
    processor.prepareToPlay (1000.0, 257);

    constexpr int totalSamples = 2400;
    std::vector<float> rendered;
    rendered.reserve (totalSamples);
    juce::MidiBuffer midi;
    int position = 0;
    std::size_t partition = 0;
    while (position < totalSamples)
    {
        const auto size = std::min (partitions[partition % partitions.size()],
                                    totalSamples - position);
        juce::AudioBuffer<float> buffer (2, size);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto absolute = position + sample;
            const auto value = static_cast<float> (0.4 * std::sin (absolute * 0.037));
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < size; ++sample)
            rendered.push_back (buffer.getSample (0, sample));
        position += size;
        ++partition;
    }
    return rendered;
}

void testDeterministicAcrossBlockPartitions()
{
    const auto regular = renderWithPartitions ({ 64 });
    const auto irregular = renderWithPartitions ({ 1, 7, 31, 129, 3, 257 });
    require (regular.size() == irregular.size(), "partition renders had different lengths");
    auto wetEnergy = 0.0;
    for (std::size_t index = 0; index < regular.size(); ++index)
    {
        requireNear (regular[index], irregular[index], 1.0e-6f,
                     "seeded output changed with host block partitioning");
        require (std::isfinite (regular[index]), "seeded render produced NaN/Inf");
        wetEnergy += std::abs (regular[index]);
    }
    require (wetEnergy > 1.0, "one-grain vertical slice produced no wet signal");
}

void testSampleRateBlockAndStressMatrix()
{
    constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr std::array<int, 6> blockSizes { 1, 7, 64, 257, 512, 1024 };
    const auto started = std::chrono::steady_clock::now();

    for (const auto sampleRate : sampleRates)
        for (const auto blockSize : blockSizes)
        {
            TextureAudioProcessor processor;
            processor.applyFactoryPreset (5);
            setActualValue (parameter (processor, "grain_size"), 5.0f);
            setActualValue (parameter (processor, "density"), 80.0f);
            setActualValue (parameter (processor, "blur"), 100.0f);
            setActualValue (parameter (processor, "detail"), 100.0f);
            setActualValue (parameter (processor, "wear"), 100.0f);
            setActualValue (parameter (processor, "dropout"), 100.0f);
            setActualValue (parameter (processor, "mix"), 100.0f);
            processor.prepareToPlay (sampleRate, blockSize);
            require (processor.getLatencySamples() == texture::SpectralSurface::getLatencySamples(),
                     "reported latency changed across the host matrix");

            juce::MidiBuffer midi;
            constexpr int totalSamples = 10000;
            int position = 0;
            while (position < totalSamples)
            {
                const auto size = std::min (blockSize, totalSamples - position);
                juce::AudioBuffer<float> buffer (2, size);
                for (int sample = 0; sample < size; ++sample)
                {
                    const auto absolute = position + sample;
                    const auto value = static_cast<float> (0.85 * std::sin (absolute * 0.029)
                                                            + 0.1 * std::sin (absolute * 0.311));
                    buffer.setSample (0, sample, value);
                    buffer.setSample (1, sample, -value * 0.7f);
                }
                processor.processBlock (buffer, midi);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < size; ++sample)
                    {
                        const auto value = buffer.getSample (channel, sample);
                        require (std::isfinite (value), "host matrix produced NaN/Inf");
                        require (std::abs (value) <= 1.001f, "safety ceiling was exceeded");
                    }
                position += size;
            }
            const auto snapshot = processor.getVisualizationSnapshot();
            require (snapshot.activeGrains <= texture::GrainEngine::maxVoices,
                     "meter snapshot exceeded the voice ceiling");
            processor.releaseResources();
        }

    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - started).count();
    require (elapsed < 30.0, "stress matrix exceeded its broad CPU safety budget");
}
} 
int main()
{
    try
    {
        testParameterContractAndState();
        testFactoryPresetsAndProgramRecall();
        testDryNullAndNonFiniteBoundary();
        testDeterministicAcrossBlockPartitions();
        testSampleRateBlockAndStressMatrix();
        std::cout << "All TEXTURE processor tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TEXTURE processor test failure: " << error.what() << '\n';
        return 1;
    }
}
