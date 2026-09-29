#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
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

juce::AudioProcessorParameter& parameterNamed (juce::AudioPluginInstance& instance,
                                                const juce::String& name)
{
    const auto iterator = std::find_if (
        instance.getParameters().begin(), instance.getParameters().end(),
        [&name] (const auto* parameter) { return parameter->getName (100) == name; });
    require (iterator != instance.getParameters().end(),
             "loaded MACHINE VST3 omitted a required parameter");
    return **iterator;
}

void configureWetStress (juce::AudioPluginInstance& instance)
{
    for (const auto& setting : std::array<std::pair<const char*, float>, 9> {{
             { "MIX", 1.0f }, { "FORCE", 0.9f }, { "MOTOR", 0.85f },
             { "RELAY", 0.9f }, { "FRICTION", 0.75f }, { "BODY", 0.8f },
             { "FEEDBACK", 0.75f }, { "DRIVE", 0.7f }, { "IRREGULARITY", 0.65f }
         }})
        parameterNamed (instance, setting.first).setValueNotifyingHost (setting.second);
}

void renderAndCheck (juce::AudioPluginInstance& instance, const int totalSamples)
{
    juce::MidiBuffer midi;
    constexpr std::array<int, 9> partitions { 1, 3, 7, 31, 64, 129, 257, 511, 13 };
    int position = 0;
    std::size_t partition = 0;
    auto energy = 0.0;
    while (position < totalSamples)
    {
        const auto size = std::min (partitions[partition % partitions.size()],
                                    totalSamples - position);
        juce::AudioBuffer<float> buffer (2, size);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto absolute = position + sample;
            const auto value = static_cast<float> (0.48 * std::sin (absolute * 0.023)
                                                    + 0.12 * std::sin (absolute * 0.173));
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, -value * 0.82f);
        }
        instance.processBlock (buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < size; ++sample)
            {
                const auto value = buffer.getSample (channel, sample);
                require (std::isfinite (value), "loaded MACHINE VST3 produced NaN/Inf");
                require (std::abs (value) <= 1.001f,
                         "loaded MACHINE VST3 exceeded its output ceiling");
                energy += std::abs (value);
            }
        position += size;
        ++partition;
    }
    require (energy > 10.0, "loaded MACHINE VST3 produced no useful audio");
}
} // namespace

int main (const int argc, char* argv[])
{
    try
    {
        require (argc == 2, "expected the built MACHINE VST3 bundle path");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        const juce::File bundle (juce::String::fromUTF8 (argv[1]));
        require (bundle.isDirectory(), "built MACHINE VST3 bundle was not found");

        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, bundle.getFullPathName());
        require (descriptions.size() == 1,
                 "VST3 scan did not return exactly one MACHINE component");
        require (descriptions[0]->name == "MACHINE", "VST3 component name changed");
        require (descriptions[0]->manufacturerName == "LSSE",
                 "VST3 manufacturer changed");

        juce::String error;
        auto instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 511, error);
        require (instance != nullptr && error.isEmpty(), "MACHINE VST3 creation failed");
        const std::set<juce::String> expectedNames {
            "MODE", "SYNC", "RATE", "STEPS", "SWING", "PROBABILITY", "RATCHET",
            "IRREGULARITY", "SEED", "FORCE", "MOTOR", "RELAY", "FRICTION", "BODY",
            "FEEDBACK", "DRIVE", "TONE", "STEREO", "MIX", "OUTPUT"
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
                 "MACHINE VST3 parameter IDs were empty or duplicated");
        for (const auto& name : expectedNames)
            require (exposedNames.contains (name), "MACHINE VST3 omitted a named parameter");
        require (! exposedNames.contains ("Program"),
                 "MACHINE exposed a host Program parameter that can reset state");
        require (instance->hasEditor(), "MACHINE VST3 omitted its editor");

        configureWetStress (*instance);
        instance->prepareToPlay (48000.0, 511);
        renderAndCheck (*instance, 96000);
        juce::MemoryBlock state;
        instance->getStateInformation (state);
        require (! state.isEmpty(), "MACHINE VST3 returned empty state");
        instance->releaseResources();
        instance.reset();

        error.clear();
        instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 511, error);
        require (instance != nullptr && error.isEmpty(), "fresh MACHINE VST3 creation failed");
        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        require (parameterNamed (*instance, "MIX").getValue() > 0.99f,
                 "MACHINE VST3 did not restore its wet mix");
        require (instance->getNumPrograms() <= 1 && instance->getCurrentProgram() == 0,
                 "MACHINE VST3 exposed a state-resetting host program");
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
        require (editor != nullptr && editor->getWidth() >= 900 && editor->getHeight() >= 680,
                 "MACHINE production editor failed to construct at its supported size");
        editor.reset();
        instance->prepareToPlay (96000.0, 511);
        renderAndCheck (*instance, 24000);
        instance->releaseResources();

        std::vector<std::unique_ptr<juce::AudioPluginInstance>> additionalInstances;
        for (int index = 0; index < 3; ++index)
        {
            error.clear();
            auto additional = format.createInstanceFromDescription (
                *descriptions[0], 48000.0, 511, error);
            require (additional != nullptr && error.isEmpty(),
                     "multiple-instance MACHINE creation failed");
            configureWetStress (*additional);
            additional->prepareToPlay (48000.0, 511);
            renderAndCheck (*additional, 12000);
            additionalInstances.push_back (std::move (additional));
        }
        for (auto& additional : additionalInstances)
            additional->releaseResources();
        std::cout << "MACHINE external VST3 host and multi-instance test passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MACHINE VST3 host test failure: " << error.what() << '\n';
        return 1;
    }
}
