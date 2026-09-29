#include "plugin/PluginProcessor.h"

#include <array>
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

juce::RangedAudioParameter& parameter (MemoryAudioProcessor& processor,
                                       const juce::String& id)
{
    for (auto* candidate : processor.getParameters())
    {
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (candidate);
            identified != nullptr && identified->paramID == id)
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (candidate))
                return *ranged;
        }
    }

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

void requireNear (const float actual, const float expected, const float tolerance,
                  const std::string_view message)
{
    if (std::abs (actual - expected) > tolerance)
        throw std::runtime_error (std::string (message));
}

void testParameterContractAndState()
{
    MemoryAudioProcessor source;
    require (source.getParameters().size() == 11, "unexpected host parameter count");

    setActualValue (parameter (source, "memoryLength"), 75.0f);
    setActualValue (parameter (source, "recall"), 82.0f);
    setActualValue (parameter (source, "age"), 91.0f);
    setActualValue (parameter (source, "fragment"), 1234.0f);
    setActualValue (parameter (source, "decay"), 67.0f);
    setActualValue (parameter (source, "corruption"), 48.0f);
    setActualValue (parameter (source, "drift"), 29.0f);
    setActualValue (parameter (source, "repeat"), 73.0f);
    setActualValue (parameter (source, "feedback"), 88.0f);
    setActualValue (parameter (source, "pastPresent"), -25.0f);
    setActualValue (parameter (source, "output"), -7.5f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    require (! state.isEmpty(), "processor state was empty");

    MemoryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    requireNear (actualValue (parameter (restored, "memoryLength")), 75.0f, 0.11f,
                 "MEMORY LENGTH was not restored");
    requireNear (actualValue (parameter (restored, "recall")), 82.0f, 0.11f,
                 "RECALL was not restored");
    requireNear (actualValue (parameter (restored, "age")), 91.0f, 0.11f,
                 "AGE was not restored");
    requireNear (actualValue (parameter (restored, "fragment")), 1234.0f, 1.1f,
                 "FRAGMENT was not restored");
    requireNear (actualValue (parameter (restored, "decay")), 67.0f, 0.11f,
                 "DECAY was not restored");
    requireNear (actualValue (parameter (restored, "corruption")), 48.0f, 0.11f,
                 "CORRUPTION was not restored");
    requireNear (actualValue (parameter (restored, "drift")), 29.0f, 0.11f,
                 "DRIFT was not restored");
    requireNear (actualValue (parameter (restored, "repeat")), 73.0f, 0.11f,
                 "REPEAT was not restored");
    requireNear (actualValue (parameter (restored, "feedback")), 88.0f, 0.11f,
                 "FEEDBACK was not restored");
    requireNear (actualValue (parameter (restored, "pastPresent")), -25.0f, 0.11f,
                 "PAST / PRESENT was not restored");
    requireNear (actualValue (parameter (restored, "output")), -7.5f, 0.11f,
                 "OUTPUT was not restored");
}

void testFactoryPresetsAndProgramState()
{
    MemoryAudioProcessor source;
    require (source.getNumPrograms() == 1, "host program contract changed");
    require (source.getNumFactoryPresets() == 6, "unexpected factory preset count");
    require (source.getFactoryPresetName (3) == "BROKEN RECALL", "factory preset name changed");

    source.applyFactoryPreset (2);
    require (source.getCurrentFactoryPreset() == 2, "current factory preset was not updated");
    requireNear (actualValue (parameter (source, "memoryLength")), 90.0f, 0.11f,
                 "OLD MEMORY did not set MEMORY LENGTH");
    requireNear (actualValue (parameter (source, "age")), 85.0f, 0.11f,
                 "OLD MEMORY did not set AGE");
    requireNear (actualValue (parameter (source, "decay")), 65.0f, 0.11f,
                 "OLD MEMORY did not set DECAY");

    juce::MemoryBlock state;
    source.getStateInformation (state);
    MemoryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    require (restored.getCurrentFactoryPreset() == 2, "factory preset identity was not restored");
    requireNear (actualValue (parameter (restored, "age")), 85.0f, 0.11f,
                 "factory preset values were not restored");
    restored.setCurrentProgram (0);
    require (restored.getCurrentFactoryPreset() == 2,
             "host Program 0 overwrote the restored factory preset");
    requireNear (actualValue (parameter (restored, "age")), 85.0f, 0.11f,
                 "host Program 0 overwrote restored parameters");
}

void testLegacyStateMigration()
{
    MemoryAudioProcessor source;
    setActualValue (parameter (source, "memoryLength"), 66.0f);
    auto legacy = source.getParameterState().copyState();
    legacy.removeProperty ("stateVersion", nullptr);
    legacy.removeProperty ("currentProgram", nullptr);

    for (const auto id : { juce::String { "repeat" }, juce::String { "feedback" },
                           juce::String { "output" } })
    {
        const auto child = legacy.getChildWithProperty ("id", id);
        if (child.isValid())
            legacy.removeChild (child, nullptr);
    }

    juce::MemoryBlock state;
    if (const auto xml = legacy.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, state);

    MemoryAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    requireNear (actualValue (parameter (restored, "memoryLength")), 66.0f, 0.11f,
                 "legacy parameter was not migrated");
    requireNear (actualValue (parameter (restored, "repeat")), 15.0f, 0.11f,
                 "missing legacy REPEAT did not keep its safe default");
    requireNear (actualValue (parameter (restored, "feedback")), 0.0f, 0.11f,
                 "missing legacy FEEDBACK did not keep its safe default");
    requireNear (actualValue (parameter (restored, "output")), 0.0f, 0.11f,
                 "missing legacy OUTPUT did not keep unity gain");
}

void testRapidAutomationAndFiniteOutput()
{
    MemoryAudioProcessor processor;
    processor.prepareToPlay (1000.0, 257);
    juce::MidiBuffer midi;
    constexpr std::array<int, 6> blockSizes { 1, 7, 31, 64, 127, 257 };
    double phase = 0.0;

    for (int cycle = 0; cycle < 200; ++cycle)
    {
        const auto blockSize = blockSizes[static_cast<std::size_t> (cycle) % blockSizes.size()];
        const auto sweep = static_cast<float> (cycle % 20) / 19.0f;
        parameter (processor, "age").setValueNotifyingHost (sweep);
        parameter (processor, "recall").setValueNotifyingHost (1.0f - sweep);
        parameter (processor, "fragment").setValueNotifyingHost (sweep);
        parameter (processor, "decay").setValueNotifyingHost (1.0f - sweep);
        parameter (processor, "corruption").setValueNotifyingHost (sweep);
        parameter (processor, "drift").setValueNotifyingHost (
            static_cast<float> ((cycle * 11) % 20) / 19.0f);
        parameter (processor, "repeat").setValueNotifyingHost (
            static_cast<float> ((cycle * 13) % 20) / 19.0f);
        parameter (processor, "feedback").setValueNotifyingHost (
            static_cast<float> ((cycle * 17) % 20) / 19.0f);
        parameter (processor, "pastPresent").setValueNotifyingHost (
            static_cast<float> ((cycle * 7) % 20) / 19.0f);
        parameter (processor, "output").setValueNotifyingHost (
            static_cast<float> ((cycle * 5) % 20) / 19.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float> (0.25 * std::sin (phase));
            phase += 0.071;
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value);
        }

        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = buffer.getSample (channel, sample);
                require (std::isfinite (value), "rapid automation produced NaN/Inf");
                require (std::abs (value) < 8.0f, "rapid automation produced runaway output");
            }
        }
    }
}

void testOutputGain()
{
    MemoryAudioProcessor processor;
    setActualValue (parameter (processor, "recall"), 0.0f);
    setActualValue (parameter (processor, "pastPresent"), 100.0f);
    setActualValue (parameter (processor, "output"), -6.0f);
    processor.prepareToPlay (1000.0, 64);

    juce::AudioBuffer<float> buffer (2, 64);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, 1.0f);

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
    const auto expected = juce::Decibels::decibelsToGain (-6.0f);
    requireNear (buffer.getSample (0, 0), expected, 1.0e-5f,
                 "OUTPUT gain did not start at the restored value");
    requireNear (buffer.getSample (1, 63), expected, 1.0e-5f,
                 "OUTPUT gain was not applied to stereo output");
}

void testLongMaximumFeedbackSoak()
{
    MemoryAudioProcessor processor;
    setActualValue (parameter (processor, "memoryLength"), 5.0f);
    setActualValue (parameter (processor, "recall"), 100.0f);
    setActualValue (parameter (processor, "fragment"), 20.0f);
    setActualValue (parameter (processor, "corruption"), 100.0f);
    setActualValue (parameter (processor, "drift"), 100.0f);
    setActualValue (parameter (processor, "repeat"), 100.0f);
    setActualValue (parameter (processor, "feedback"), 95.0f);
    setActualValue (parameter (processor, "pastPresent"), 0.0f);
    setActualValue (parameter (processor, "output"), 6.0f);
    processor.prepareToPlay (1000.0, 257);

    constexpr std::array<int, 5> blockSizes { 1, 17, 63, 128, 257 };
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int cycle = 0; cycle < 2000; ++cycle)
    {
        const auto blockSize = blockSizes[static_cast<std::size_t> (cycle) % blockSizes.size()];
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float> (0.5 * std::sin (phase));
            phase += 0.113;
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value);
        }

        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = buffer.getSample (channel, sample);
                require (std::isfinite (value), "maximum-feedback soak produced NaN/Inf");
                require (std::abs (value) < 16.0f, "maximum-feedback soak ran away");
            }
    }
}

void testVisualizationSnapshotAndEditor()
{
    MemoryAudioProcessor processor;
    processor.prepareToPlay (1000.0, 128);
    juce::AudioBuffer<float> buffer (2, 100);
    buffer.clear();
    buffer.applyGain (0.5f);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, 0.5f);

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
    const auto snapshot = processor.getVisualizationSnapshot();
    require (snapshot.writeIndex > 0, "visualization write position did not advance");

    auto hasActivity = false;
    for (const auto value : snapshot.activity)
    {
        require (std::isfinite (value), "visualization produced NaN/Inf");
        require (value >= 0.0f && value <= 1.0f, "visualization escaped its bounded range");
        hasActivity = hasActivity || value > 0.0f;
    }
    require (hasActivity, "visualization did not capture input activity");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    require (editor != nullptr, "custom editor was not created");
    require (editor->getWidth() == 780 && editor->getHeight() == 610,
             "custom editor opened at an unexpected size");
}
} 
int main()
{
    try
    {
        testParameterContractAndState();
        testFactoryPresetsAndProgramState();
        testLegacyStateMigration();
        testRapidAutomationAndFiniteOutput();
        testOutputGain();
        testLongMaximumFeedbackSoak();
        testVisualizationSnapshotAndEditor();
        std::cout << "All processor tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Processor test failure: " << error.what() << '\n';
        return 1;
    }
}
