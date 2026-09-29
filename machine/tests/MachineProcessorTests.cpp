#include "machine/plugin/MachineProcessor.h"
#include "machine/plugin/MachineEditor.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>
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

juce::RangedAudioParameter& parameter (MachineAudioProcessor& processor,
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
    MachineAudioProcessor source;
    require (source.getParameters().size() == 24, "unexpected MACHINE parameter count");
    const std::set<juce::String> expectedIds {
        "mode", "sync", "rate", "steps", "swing", "probability", "ratchet",
        "irregularity", "seed", "force", "motor", "relay", "friction", "body",
        "feedback", "drive", "tone", "stereo", "mix", "output",
        "sample_layer", "sample_blend", "sample_pitch", "sample_loop"
    };
    std::set<juce::String> actualIds;
    for (auto* candidate : source.getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (candidate))
            actualIds.insert (identified->paramID);
    require (actualIds == expectedIds, "stable MACHINE parameter IDs changed");

    setActualValue (parameter (source, "steps"), 9.0f);
    setActualValue (parameter (source, "swing"), 37.0f);
    setActualValue (parameter (source, "seed"), 9001.0f);
    setActualValue (parameter (source, "mix"), 74.0f);
    setActualValue (parameter (source, "friction"), 63.0f);
    setActualValue (parameter (source, "feedback"), 41.0f);
    setActualValue (parameter (source, "tone"), -27.0f);
    juce::MemoryBlock state;
    source.getStateInformation (state);
    require (! state.isEmpty(), "MACHINE returned empty state");

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(),
                                                        static_cast<int> (state.getSize()));
    require (xml != nullptr, "MACHINE state was not XML");
    auto tree = juce::ValueTree::fromXml (*xml);
    auto pattern = tree.getChildWithName ("PATTERN");
    require (pattern.isValid() && pattern.getNumChildren() == 16,
             "MACHINE state omitted its 16-step pattern");
    pattern.getChild (3).setProperty ("accent", 0.75f, nullptr);
    pattern.getChild (3).setProperty ("ratchet", 4, nullptr);
    if (const auto changedXml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary (*changedXml, state);

    MachineAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    require (std::abs (actualValue (parameter (restored, "steps")) - 9.0f) < 0.1f,
             "STEPS was not restored");
    require (std::abs (actualValue (parameter (restored, "swing")) - 37.0f) < 0.11f,
             "SWING was not restored");
    require (std::abs (actualValue (parameter (restored, "seed")) - 9001.0f) < 0.1f,
             "SEED was not restored");
    require (std::abs (actualValue (parameter (restored, "friction")) - 63.0f) < 0.11f
             && std::abs (actualValue (parameter (restored, "feedback")) - 41.0f) < 0.11f
             && std::abs (actualValue (parameter (restored, "tone")) + 27.0f) < 0.11f,
             "phase-4 controls were not restored");
    require (std::abs (restored.getPatternForTests().steps[3].accent - 0.75f) < 1.0e-6f
             && restored.getPatternForTests().steps[3].ratchet == 4,
             "pattern data was not restored");
}

void testTransparentShellAndEditor()
{
    MachineAudioProcessor processor;
    processor.prepareToPlay (48000.0, 257);
    juce::MidiBuffer midi;
    constexpr std::array<int, 7> sizes { 1, 7, 31, 64, 129, 257, 13 };
    int position = 0;
    std::size_t partition = 0;
    while (position < 4000)
    {
        const auto size = std::min (sizes[partition % sizes.size()], 4000 - position);
        juce::AudioBuffer<float> buffer (2, size);
        std::vector<float> left (static_cast<std::size_t> (size));
        std::vector<float> right (static_cast<std::size_t> (size));
        for (int sample = 0; sample < size; ++sample)
        {
            left[static_cast<std::size_t> (sample)] = static_cast<float> (
                0.4 * std::sin ((position + sample) * 0.031));
            right[static_cast<std::size_t> (sample)] = -left[static_cast<std::size_t> (sample)];
            buffer.setSample (0, sample, left[static_cast<std::size_t> (sample)]);
            buffer.setSample (1, sample, right[static_cast<std::size_t> (sample)]);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < size; ++sample)
        {
            require (buffer.getSample (0, sample) == left[static_cast<std::size_t> (sample)],
                     "phase-2 shell changed left-channel audio");
            require (buffer.getSample (1, sample) == right[static_cast<std::size_t> (sample)],
                     "phase-2 shell changed right-channel audio");
        }
        position += size;
        ++partition;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    require (editor != nullptr, "MACHINE diagnostic editor was not created");
    require (dynamic_cast<MachineAudioProcessorEditor*> (editor.get()) != nullptr,
             "MACHINE custom editor was not created");
    require (editor->getWidth() >= 900 && editor->getHeight() >= 680,
             "MACHINE editor opened below its usable size");
}

void testFactoryPresetsAndUiPattern()
{
    MachineAudioProcessor source;
    require (source.getNumFactoryPresets() == 8, "unexpected MACHINE preset count");
    require (source.getFactoryPresetName (4) == "BROKEN CLOCK",
             "MACHINE preset name changed");
    source.applyFactoryPreset (4);
    require (source.getCurrentFactoryPreset() == 4, "MACHINE preset was not selected");
    require (std::abs (actualValue (parameter (source, "feedback")) - 58.0f) < 0.11f,
             "MACHINE preset parameters were not applied");
    const auto edited = source.getPatternForUi();
    require (edited.activeSteps == 7 && edited.steps[0].probability < 1.0f,
             "MACHINE preset pattern was not applied");

    auto custom = edited;
    custom.revision += 10;
    custom.steps[2].value = 0.123f;
    custom.steps[2].accent = 0.987f;
    require (source.submitPatternFromUi (custom), "MACHINE rejected UI pattern snapshot");
    juce::MemoryBlock state;
    source.getStateInformation (state);
    MachineAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    require (restored.getCurrentFactoryPreset() == 4,
             "MACHINE selected preset was not restored");
    const auto restoredPattern = restored.getPatternForUi();
    require (std::abs (restoredPattern.steps[2].value - 0.123f) < 1.0e-6f
             && std::abs (restoredPattern.steps[2].accent - 0.987f) < 1.0e-6f,
             "MACHINE UI pattern was not restored");
}

std::vector<float> renderWet (const std::vector<int>& partitions)
{
    MachineAudioProcessor processor;
    setActualValue (parameter (processor, "mix"), 100.0f);
    setActualValue (parameter (processor, "motor"), 85.0f);
    setActualValue (parameter (processor, "relay"), 90.0f);
    setActualValue (parameter (processor, "force"), 80.0f);
    setActualValue (parameter (processor, "friction"), 72.0f);
    setActualValue (parameter (processor, "body"), 68.0f);
    setActualValue (parameter (processor, "feedback"), 38.0f);
    setActualValue (parameter (processor, "drive"), 55.0f);
    setActualValue (parameter (processor, "tone"), 18.0f);
    setActualValue (parameter (processor, "irregularity"), 35.0f);
    setActualValue (parameter (processor, "ratchet"), 3.0f);
    setActualValue (parameter (processor, "seed"), 1234.0f);
    processor.prepareToPlay (48000.0, 257);

    constexpr int totalSamples = 36000;
    std::vector<float> output;
    output.reserve (totalSamples);
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
            const auto value = static_cast<float> (0.55 * std::sin (absolute * 0.037)
                                                    + 0.1 * std::sin (absolute * 0.19));
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value * 0.8f);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto value = buffer.getSample (0, sample);
            require (std::isfinite (value), "MACHINE wet render produced NaN/Inf");
            require (std::abs (value) <= 1.001f, "MACHINE wet render exceeded its ceiling");
            output.push_back (value);
        }
        position += size;
        ++partition;
    }
    return output;
}

void testWetDeterminismAcrossPartitions()
{
    const auto regular = renderWet ({ 64 });
    const auto irregular = renderWet ({ 1, 7, 31, 129, 3, 257 });
    require (regular.size() == irregular.size(), "MACHINE wet renders changed length");
    auto energy = 0.0;
    for (std::size_t index = 0; index < regular.size(); ++index)
    {
        require (std::abs (regular[index] - irregular[index]) < 1.0e-6f,
                 "MACHINE audio changed with host block partitioning");
        energy += std::abs (regular[index]);
    }
    require (energy > 5.0, "MACHINE wet render produced no mechanism audio");
}

void testLoadedLayerAndState()
{
    const auto file = juce::File::createTempFile ("lsse-machine-layer.wav");
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    require (stream != nullptr, "could not create MACHINE sample fixture");
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.release(), 48000.0, 1, 16, {}, 0));
    require (writer != nullptr, "could not create MACHINE sample writer");
    juce::AudioBuffer<float> source (1, 2048);
    for (int sample = 0; sample < source.getNumSamples(); ++sample)
        source.setSample (0, sample, 0.45f * std::sin (sample * 0.17f));
    require (writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples()),
             "could not write MACHINE sample fixture");
    writer.reset();

    MachineAudioProcessor withSample, procedural;
    for (auto* processor : { &withSample, &procedural })
    {
        setActualValue (parameter (*processor, "mix"), 100.0f);
        setActualValue (parameter (*processor, "sample_layer"), 1.0f);
        setActualValue (parameter (*processor, "sample_blend"), 100.0f);
        setActualValue (parameter (*processor, "sample_loop"), 1.0f);
        processor->prepareToPlay (48000.0, 257);
    }
    require (withSample.loadLayerSample (file), "MACHINE rejected a valid layer sample");
    juce::AudioBuffer<float> loadedRender (2, 257), proceduralRender (2, 257);
    loadedRender.clear(); proceduralRender.clear();
    juce::MidiBuffer midi;
    withSample.processBlock (loadedRender, midi);
    procedural.processBlock (proceduralRender, midi);
    double difference = 0.0;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < loadedRender.getNumSamples(); ++sample)
            difference += std::abs (loadedRender.getSample (channel, sample)
                                   - proceduralRender.getSample (channel, sample));
    require (difference > 0.1, "loaded MACHINE layer did not affect audio");

    juce::MemoryBlock state;
    withSample.getStateInformation (state);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(),
                                                        static_cast<int> (state.getSize()));
    require (xml != nullptr && xml->getStringAttribute ("layerSamplePath")
                                  == file.getFullPathName(),
             "MACHINE state omitted its layer sample path");
    file.deleteFile();
}

void testHostileAutomationAndMultipleInstances()
{
    std::array<std::unique_ptr<MachineAudioProcessor>, 4> processors;
    for (auto& processor : processors)
    {
        processor = std::make_unique<MachineAudioProcessor>();
        setActualValue (parameter (*processor, "mix"), 100.0f);
        processor->prepareToPlay (48000.0, 257);
    }
    const auto started = std::chrono::steady_clock::now();
    juce::MidiBuffer midi;
    for (int block = 0; block < 480; ++block)
    {
        for (std::size_t instance = 0; instance < processors.size(); ++instance)
        {
            auto& processor = *processors[instance];
            setActualValue (parameter (processor, "feedback"), block % 2 == 0 ? 95.0f : 0.0f);
            setActualValue (parameter (processor, "drive"), static_cast<float> ((block * 17) % 101));
            setActualValue (parameter (processor, "tone"), static_cast<float> ((block * 29) % 201 - 100));
            setActualValue (parameter (processor, "ratchet"), static_cast<float> (1 + block % 4));
            setActualValue (parameter (processor, "irregularity"), static_cast<float> ((block * 11) % 101));
            const auto size = 1 + (block * 67 + static_cast<int> (instance) * 31) % 257;
            juce::AudioBuffer<float> buffer (2, size);
            for (int sample = 0; sample < size; ++sample)
            {
                const auto value = static_cast<float> (
                    0.7 * std::sin ((block * 257 + sample) * 0.031));
                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, -value);
            }
            processor.processBlock (buffer, midi);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < size; ++sample)
                {
                    const auto value = buffer.getSample (channel, sample);
                    require (std::isfinite (value), "hostile automation produced NaN/Inf");
                    require (std::abs (value) <= 1.001f,
                             "hostile automation escaped the output ceiling");
                }
        }
    }
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - started).count();
#if defined(NDEBUG)
    require (elapsed < 15.0, "release multi-instance stress exceeded its CPU budget");
#else
    require (elapsed < 90.0, "debug multi-instance stress exceeded its CPU budget");
#endif
}
} 
int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI initialiser;
        testParameterContractAndState();
        testTransparentShellAndEditor();
        testFactoryPresetsAndUiPattern();
        testWetDeterminismAcrossPartitions();
        testLoadedLayerAndState();
        testHostileAutomationAndMultipleInstances();
        std::cout << "All MACHINE processor tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MACHINE processor test failure: " << error.what() << '\n';
        return 1;
    }
}
