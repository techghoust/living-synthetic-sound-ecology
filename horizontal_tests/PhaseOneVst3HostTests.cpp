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
struct Product { const char* name; int parameterCount; };
constexpr std::array<Product, 6> products {{
    { "MATERIAL", 20 }, { "IMPACT", 26 }, { "CREATURE", 24 },
    { "MOTION", 18 }, { "ENVIRONMENT", 25 }, { "CONVOLUTION", 17 }
}};

void require (bool condition, std::string_view message)
{
    if (! condition) throw std::runtime_error (std::string (message));
}

void verifyProduct (juce::VST3PluginFormat& format, const Product& product, const juce::File& bundle)
{
    std::cout << "Testing " << product.name << "...\n" << std::flush;
    require (bundle.isDirectory(), "built VST3 bundle was not found");
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, bundle.getFullPathName());
    require (descriptions.size() == 1, "VST3 scan did not return exactly one component");
    require (descriptions[0]->name == product.name, "VST3 component name changed");
    require (descriptions[0]->manufacturerName == "LSSE", "VST3 manufacturer changed");

    juce::String error;
    auto instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
    require (instance != nullptr && error.isEmpty(), "VST3 instance creation failed");
    std::cout << "  instance\n" << std::flush;
    require (instance->getNumPrograms() <= 1 && instance->getCurrentProgram() == 0, "unexpected program model");
    require (instance->hasEditor(), "VST3 did not expose an editor");

    std::set<juce::String> ids, names;
    for (auto* parameter : instance->getParameters())
    {
        if (const auto* identified = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter)) ids.insert (identified->getParameterID());
        names.insert (parameter->getName (100));
    }
    require (ids.size() == static_cast<std::size_t> (instance->getParameters().size()), "parameter IDs were empty or duplicated");
    require (instance->getParameters().size() == product.parameterCount + 1, "parameter contract count changed");
    require (! names.contains ("Program"), "host Program parameter can overwrite restored state");
    std::cout << "  parameters\n" << std::flush;

    const auto stateProbeIt = std::find_if (instance->getParameters().begin(), instance->getParameters().end(),
                                           [] (const auto* parameter) { return parameter->getName (100) == "MIX"; });
    require (stateProbeIt != instance->getParameters().end(), "MIX state probe was not exposed");
    auto* stateProbe = *stateProbeIt;

    instance->prepareToPlay (48000.0, 257);
    juce::MidiBuffer midi;
    constexpr std::array<int, 7> partitions { 1, 7, 31, 64, 129, 257, 13 };
    int position = 0;
    std::size_t partition = 0;
    while (position < 4096)
    {
        const auto size = std::min (partitions[partition++ % partitions.size()], 4096 - position);
        juce::AudioBuffer<float> buffer (2, size), original (2, size);
        for (int sample = 0; sample < size; ++sample)
        {
            const auto value = static_cast<float> (0.4 * std::sin ((position + sample) * 0.027));
            buffer.setSample (0, sample, value); buffer.setSample (1, sample, -value);
        }
        original.makeCopyOf (buffer);
        instance->processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < size; ++sample)
                require (buffer.getSample (channel, sample) == original.getSample (channel, sample), "Phase 1 is not exact pass-through");
        position += size;
    }
    std::cout << "  dry path\n" << std::flush;

    stateProbe->setValueNotifyingHost (1.0f);
    // Never pass a block larger than the maximum declared in prepareToPlay.
    juce::AudioBuffer<float> audible (2, 257), dryAudible (2, 257);
    for (int sample = 0; sample < audible.getNumSamples(); ++sample)
    {
        const auto value = 0.5f * std::sin (sample * 0.071f);
        audible.setSample (0, sample, value); audible.setSample (1, sample, -value * 0.8f);
    }
    dryAudible.makeCopyOf (audible);
    instance->processBlock (audible, midi);
    double difference = 0.0;
    for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < audible.getNumSamples(); ++sample)
        difference += std::abs (audible.getSample (channel, sample) - dryAudible.getSample (channel, sample));
    require (difference > 0.01, "Phase 3 prototype did not produce an audible transform at MIX 100%");
    std::cout << "  wet path\n" << std::flush;

    stateProbe->setValueNotifyingHost (0.731f);
    const auto expectedRestoredValue = stateProbe->getValue();

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    std::cout << "  state captured\n" << std::flush;
    require (! state.isEmpty(), "VST3 returned empty state");
    instance->releaseResources();
    instance.reset();

    error.clear();
    instance = format.createInstanceFromDescription (*descriptions[0], 48000.0, 257, error);
    require (instance != nullptr && error.isEmpty(), "fresh VST3 instance creation failed");
    instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    const auto restoredProbeIt = std::find_if (instance->getParameters().begin(), instance->getParameters().end(),
                                              [] (const auto* parameter) { return parameter->getName (100) == "MIX"; });
    require (restoredProbeIt != instance->getParameters().end(), "restored MIX state probe was not exposed");
    require (std::abs ((*restoredProbeIt)->getValue() - expectedRestoredValue) < 0.001f, "parameter state did not restore");
    std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
    require (editor != nullptr, "editor construction failed");
    std::cout << product.name << " Phase 6 host test passed\n";
}
}

int main (int argc, char* argv[])
{
    try
    {
        require (argc == 7, "expected six VST3 bundle paths");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::VST3PluginFormat format;
        for (std::size_t i = 0; i < products.size(); ++i)
            verifyProduct (format, products[i], juce::File (juce::String::fromUTF8 (argv[i + 1])));
        std::cout << "All six LSSE Phase 6 VST3 products passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "LSSE Phase 6 VST3 host test failure: " << error.what() << '\n';
        return 1;
    }
}
