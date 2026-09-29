#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
}

int main (const int argc, char* argv[])
{
    try
    {
        require (argc == 2, "expected the built VST3 bundle path");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        const juce::File bundle (juce::String::fromUTF8 (argv[1]));
        require (bundle.isDirectory(), "built VST3 bundle was not found");

        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, bundle.getFullPathName());
        require (descriptions.size() == 1, "VST3 scan did not return exactly one component");
        require (descriptions[0]->name == "TEXTURE", "VST3 component name changed");
        require (descriptions[0]->manufacturerName == "LSSE",
                 "VST3 manufacturer changed");

        juce::String error;
        auto instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
        require (instance != nullptr, "VST3 instance creation failed");
        require (error.isEmpty(), "VST3 host returned a creation error");
        const std::set<juce::String> expectedNames {
            "INPUT", "CAPTURE", "GRAIN SIZE", "DENSITY", "SPRAY", "PITCH", "REVERSE",
            "BLUR", "FREEZE", "DETAIL", "WEAR", "DROPOUT", "MOTION RATE", "MOTION DEPTH",
            "TONE", "WIDTH", "SEED", "MIX", "OUTPUT"
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
            require (exposedNames.contains (name), "VST3 omitted a named TEXTURE parameter");
        require (! exposedNames.contains ("Program"),
                 "VST3 exposed a host Program parameter that can overwrite restored state");
        require (instance->hasEditor(), "VST3 did not expose its production editor");

        instance->prepareToPlay (48000.0, 257);
        require (instance->getLatencySamples() == 512, "VST3 reported the wrong latency");
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

        juce::MemoryBlock state;
        instance->getStateInformation (state);
        require (! state.isEmpty(), "loaded VST3 returned empty state");
        instance->releaseResources();
        instance.reset();

        error.clear();
        instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
        require (instance != nullptr, "fresh VST3 instance creation failed");
        require (error.isEmpty(), "fresh VST3 instance returned a creation error");
        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        require (instance->getNumPrograms() <= 1 && instance->getCurrentProgram() == 0,
                 "VST3 exposed a host Program parameter that can overwrite restored state");

        auto* density = std::find_if (instance->getParameters().begin(),
                                      instance->getParameters().end(),
                                      [] (const auto* parameter)
                                      {
                                          return parameter->getName (100) == "DENSITY";
                                      });
        require (density != instance->getParameters().end(),
                 "fresh VST3 instance omitted DENSITY");
        require (std::abs ((*density)->getCurrentValueAsText().getFloatValue() - 8.0f) < 0.11f,
                 "fresh VST3 instance did not restore parameters");

        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
        require (editor != nullptr && editor->getWidth() >= 820 && editor->getHeight() >= 640,
                 "VST3 production editor failed to construct at its supported size");
        editor.reset();
        instance->releaseResources();
        std::cout << "TEXTURE external VST3 host smoke test passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TEXTURE VST3 host test failure: " << error.what() << '\n';
        return 1;
    }
}
