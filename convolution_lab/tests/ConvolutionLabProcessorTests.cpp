#include "convolution_lab/plugin/ConvolutionLabProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require (const bool condition, const char* message)
{
    if (! condition)
        throw std::runtime_error (message);
}

void setParameter (juce::AudioProcessor& processor, const juce::String& name, const float value)
{
    for (auto* parameter : processor.getParameters())
        if (parameter->getName (100) == name)
        {
            parameter->setValueNotifyingHost (value);
            return;
        }
    throw std::runtime_error ("parameter not found: " + name.toStdString());
}

juce::File createTestImpulse()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getNonexistentChildFile ("lsse-convolution-test", ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    require (stream != nullptr, "could not create temporary IR");
    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions {}.withSampleRate (48000.0)
                                                      .withNumChannels (1)
                                                      .withBitsPerSample (24);
    auto writer = format.createWriterFor (stream, options);
    require (writer != nullptr, "could not create temporary IR writer");
    juce::AudioBuffer<float> impulse (1, 4800);
    impulse.clear();
    impulse.setSample (0, 0, 1.0f);
    impulse.setSample (0, 1200, 0.5f);
    impulse.setSample (0, 3600, -0.2f);
    require (writer->writeFromAudioSampleBuffer (impulse, 0, impulse.getNumSamples()),
             "could not write temporary IR");
    writer.reset();
    return file;
}
}

int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        const auto impulseFile = createTestImpulse();
        ConvolutionLabAudioProcessor processor;
        processor.prepareToPlay (48000.0, 257);
        require (processor.loadImpulseResponseForSlot (0, impulseFile),
                 "valid WAV IR did not load");
        require (processor.getImpulseAsset (0) == impulseFile.getFullPathName(),
                 "loaded IR path was not retained");
        setParameter (processor, "MIX", 1.0f);
        setParameter (processor, "MORPH", 0.0f);

        juce::MidiBuffer midi;
        double outputEnergy = 0.0;
        for (int block = 0; block < 32; ++block)
        {
            juce::AudioBuffer<float> audio (2, 257);
            audio.clear();
            if (block == 0)
                audio.setSample (0, 0, 1.0f);
            processor.processBlock (audio, midi);
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                {
                    const auto value = audio.getSample (channel, sample);
                    require (std::isfinite (value), "loaded IR produced non-finite output");
                    outputEnergy += std::abs (value);
                }
        }
        require (outputEnergy > 0.05, "loaded IR produced no audio");

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        ConvolutionLabAudioProcessor restored;
        restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        require (restored.getImpulseAsset (0) == impulseFile.getFullPathName(),
                 "IR path was not restored from project state");
        restored.prepareToPlay (48000.0, 257);

        require (impulseFile.deleteFile(), "temporary IR cleanup failed");
        ConvolutionLabAudioProcessor missing;
        missing.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        missing.prepareToPlay (48000.0, 257);
        juce::AudioBuffer<float> fallback (2, 257);
        fallback.clear();
        fallback.setSample (0, 0, 1.0f);
        missing.processBlock (fallback, midi);
        for (int channel = 0; channel < fallback.getNumChannels(); ++channel)
            for (int sample = 0; sample < fallback.getNumSamples(); ++sample)
                require (std::isfinite (fallback.getSample (channel, sample)),
                         "missing IR fallback produced non-finite output");

        std::cout << "CONVOLUTION file IR, state and missing fallback tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CONVOLUTION processor test failure: " << error.what() << '\n';
        return 1;
    }
}
