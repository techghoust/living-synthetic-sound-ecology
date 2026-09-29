#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
struct Product { const char* name; };
constexpr std::array<Product, 6> products {{
    { "MATERIAL" }, { "IMPACT" }, { "CREATURE" },
    { "MOTION" }, { "ENVIRONMENT" }, { "CONVOLUTION" }
}};
constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
constexpr std::array<int, 5> blockSizes { 1, 7, 64, 257, 1024 };

void require (bool condition, std::string_view message)
{
    if (! condition) throw std::runtime_error (std::string (message));
}

std::unique_ptr<juce::AudioPluginInstance> createInstance (juce::VST3PluginFormat& format,
                                                            const juce::PluginDescription& description,
                                                            double sampleRate, int blockSize)
{
    juce::String error;
    auto instance = format.createInstanceFromDescription (description, sampleRate, blockSize, error);
    require (instance != nullptr && error.isEmpty(), "instance creation failed");
    return instance;
}

void setParameter (juce::AudioPluginInstance& instance, const juce::String& name, float value)
{
    for (auto* parameter : instance.getParameters())
        if (parameter->getName (100) == name)
        {
            parameter->setValueNotifyingHost (value);
            return;
        }
}

void automate (juce::AudioPluginInstance& instance, int blockIndex)
{
    int parameterIndex = 0;
    for (auto* parameter : instance.getParameters())
    {
        if (parameter->getName (100).containsIgnoreCase ("bypass"))
        {
            parameter->setValueNotifyingHost (0.0f);
            continue;
        }
        const auto pattern = (blockIndex * 17 + parameterIndex * 11) % 5;
        const auto value = std::array<float, 5> { 0.0f, 1.0f, 0.001f, 0.999f, 0.5f }[(std::size_t) pattern];
        parameter->setValueNotifyingHost (value);
        ++parameterIndex;
    }
    setParameter (instance, "MIX", blockIndex % 3 == 0 ? 0.0f : 1.0f);
}

void exerciseConfiguration (juce::VST3PluginFormat& format, const juce::PluginDescription& description,
                            double sampleRate, int blockSize)
{
    auto instance = createInstance (format, description, sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio (2, blockSize);
    std::uint64_t timeline = 0;
    for (int block = 0; block < 24; ++block)
    {
        automate (*instance, block);
        for (int sample = 0; sample < blockSize; ++sample, ++timeline)
        {
            float value = static_cast<float> (0.65 * std::sin (timeline * 0.019));
            if (timeline % 509 == 0) value = timeline % 1018 == 0 ? 3.5f : -3.5f;
            if (timeline % 997 == 0) value = std::numeric_limits<float>::quiet_NaN();
            audio.setSample (0, sample, value);
            audio.setSample (1, sample, -value * 0.83f);
        }
        instance->processBlock (audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = audio.getSample (channel, sample);
                if (! std::isfinite (value) || std::abs (value) >= 64.0f)
                    throw std::runtime_error (
                        description.name.toStdString() + " failed at "
                        + std::to_string (sampleRate) + " Hz, block size "
                        + std::to_string (blockSize) + ", automation block "
                        + std::to_string (block) + ", channel "
                        + std::to_string (channel) + ", sample "
                        + std::to_string (sample) + ": output="
                        + std::to_string (value));
            }
    }
    juce::MemoryBlock state;
    instance->getStateInformation (state);
    require (! state.isEmpty() && state.getSize() < 1024 * 1024, "state size was empty or excessive");
    instance->releaseResources();
}

void exerciseMultipleInstances (juce::VST3PluginFormat& format, const juce::PluginDescription& description)
{
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> instances;
    for (int i = 0; i < 4; ++i)
    {
        auto instance = createInstance (format, description, 48000.0, 257);
        instance->prepareToPlay (48000.0, 257);
        setParameter (*instance, "MIX", 1.0f);
        instances.push_back (std::move (instance));
    }
    juce::MidiBuffer midi;
    for (int block = 0; block < 16; ++block)
        for (std::size_t index = 0; index < instances.size(); ++index)
        {
            juce::AudioBuffer<float> audio (2, 257);
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = static_cast<float> (0.4 * std::sin ((block * 257 + sample) * (0.01 + index * 0.003)));
                audio.setSample (0, sample, value); audio.setSample (1, sample, -value);
            }
            instances[index]->processBlock (audio, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    require (std::isfinite (audio.getSample (channel, sample)), "multi-instance output was non-finite");
        }
}
}

int main (int argc, char* argv[])
{
    try
    {
        require (argc == 7, "expected six VST3 bundle paths");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::VST3PluginFormat format;
        const auto started = std::chrono::steady_clock::now();
        double renderedSeconds = 0.0;
        for (std::size_t productIndex = 0; productIndex < products.size(); ++productIndex)
        {
            juce::OwnedArray<juce::PluginDescription> descriptions;
            format.findAllTypesForFile (descriptions, juce::String::fromUTF8 (argv[productIndex + 1]));
            require (descriptions.size() == 1, "VST3 scan failed");
            require (descriptions[0]->name == products[productIndex].name, "product identity changed");
            for (const auto sampleRate : sampleRates)
                for (const auto blockSize : blockSizes)
                {
                    exerciseConfiguration (format, *descriptions[0], sampleRate, blockSize);
                    renderedSeconds += 24.0 * blockSize / sampleRate;
                }
            exerciseMultipleInstances (format, *descriptions[0]);
            std::cout << products[productIndex].name << " stress matrix passed\n";
        }
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
        require (elapsed < renderedSeconds * 50.0 + 10.0, "stress matrix exceeded generous realtime budget");
        std::cout << "LSSE Phase 6 stress matrix passed in " << elapsed << " s\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "LSSE Phase 6 stress failure: " << error.what() << '\n';
        return 1;
    }
}
