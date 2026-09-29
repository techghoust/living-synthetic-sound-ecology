#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>

namespace
{
void require (const bool condition, const std::string_view message)
{
    if (! condition)
        throw std::runtime_error (std::string (message));
}

juce::AudioProcessorParameter& parameterNamed (juce::AudioPluginInstance& instance,
                                                const juce::String& name)
{
    for (auto* candidate : instance.getParameters())
        if (candidate->getName (100) == name)
            return *candidate;

    throw std::runtime_error ("parameter not found: " + name.toStdString());
}
}

int main (const int argc, char* argv[])
{
    try
    {
        require (argc == 2, "expected the built VST3 bundle path");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        const juce::File bundle (juce::String::fromUTF8 (argv[1]));
        require (bundle.isDirectory(), "built MEMORY VST3 bundle was not found");

        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, bundle.getFullPathName());
        require (descriptions.size() == 1, "VST3 scan did not return exactly one component");
        require (descriptions[0]->name == "MEMORY", "VST3 component name changed");
        require (descriptions[0]->manufacturerName == "LSSE",
                 "VST3 manufacturer changed");

        juce::String error;
        auto instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
        require (instance != nullptr && error.isEmpty(), "VST3 instance creation failed");

        const std::set<juce::String> expectedNames {
            "MEMORY LENGTH", "RECALL", "AGE", "FRAGMENT", "DECAY", "CORRUPTION",
            "DRIFT", "REPEAT", "FEEDBACK", "PAST / PRESENT", "OUTPUT"
        };
        std::set<juce::String> exposedIds;
        std::set<juce::String> exposedNames;
        for (auto* parameter : instance->getParameters())
        {
            if (const auto* identified = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
                exposedIds.insert (identified->getParameterID());
            exposedNames.insert (parameter->getName (100));
        }
        require (exposedIds.size() == static_cast<std::size_t> (instance->getParameters().size()),
                 "VST3 parameter IDs were empty or duplicated");
        for (const auto& name : expectedNames)
            require (exposedNames.contains (name), "VST3 omitted a named MEMORY parameter");
        require (! exposedNames.contains ("Program"),
                 "VST3 exposed a host Program parameter that can overwrite restored state");
        require (instance->hasEditor(), "VST3 did not expose its production editor");

        instance->prepareToPlay (48000.0, 257);
        juce::MidiBuffer midi;
        constexpr std::array<int, 7> partitions { 1, 7, 31, 64, 129, 257, 13 };
        int position = 0;
        std::size_t partition = 0;
        while (position < 6000)
        {
            const auto size = std::min (partitions[partition % partitions.size()], 6000 - position);
            juce::AudioBuffer<float> buffer (2, size);
            for (int sample = 0; sample < size; ++sample)
            {
                const auto value = static_cast<float> (0.4 * std::sin ((position + sample) * 0.027));
                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, -value);
            }
            instance->processBlock (buffer, midi);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < size; ++sample)
                    require (std::isfinite (buffer.getSample (channel, sample)),
                             "loaded VST3 produced NaN/Inf");
            position += size;
            ++partition;
        }

        // Verify the actual recalled-audio path rather than only checking that
        // input reaches the visualiser and the dry output remains finite.
        parameterNamed (*instance, "RECALL").setValueNotifyingHost (1.0f);
        parameterNamed (*instance, "MEMORY LENGTH").setValueNotifyingHost (0.0f);
        parameterNamed (*instance, "FRAGMENT").setValueNotifyingHost (0.35f);
        parameterNamed (*instance, "PAST / PRESENT").setValueNotifyingHost (0.0f);
        double recalledEnergy = 0.0;
        int recalledTimeline = 0;
        while (recalledTimeline < 96000)
        {
            const auto size = std::min (257, 96000 - recalledTimeline);
            juce::AudioBuffer<float> buffer (2, size);
            for (int sample = 0; sample < size; ++sample)
            {
                const auto timeline = recalledTimeline + sample;
                const auto value = static_cast<float> (0.31 * std::sin (timeline * 0.019)
                                                       + 0.13 * std::sin (timeline * 0.071));
                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, -value * 0.8f);
            }
            instance->processBlock (buffer, midi);
            if (recalledTimeline >= 4800)
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    for (int sample = 0; sample < size; ++sample)
                        recalledEnergy += std::abs (buffer.getSample (channel, sample));
            recalledTimeline += size;
        }
        require (recalledEnergy > 10.0,
                 "MEMORY visualiser reacted but recalled-audio path was silent");

        juce::MemoryBlock state;
        instance->getStateInformation (state);
        require (! state.isEmpty(), "loaded VST3 returned empty state");
        instance->releaseResources();
        instance.reset();

        error.clear();
        instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
        require (instance != nullptr && error.isEmpty(), "fresh VST3 instance creation failed");
        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        require (instance->getNumPrograms() <= 1 && instance->getCurrentProgram() == 0,
                 "VST3 exposed a host Program parameter that can overwrite restored state");

        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
        require (editor != nullptr && editor->getWidth() == 780 && editor->getHeight() == 610,
                 "VST3 production editor failed to construct");
        editor.reset();
        instance->releaseResources();
        std::cout << "MEMORY external VST3 host smoke test passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MEMORY VST3 host test failure: " << error.what() << '\n';
        return 1;
    }
}
