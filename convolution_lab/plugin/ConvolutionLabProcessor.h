#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class ConvolutionLabAudioProcessor final : public juce::AudioProcessor,
                                           private juce::AsyncUpdater
{
public:
    ConvolutionLabAudioProcessor();
    ~ConvolutionLabAudioProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) override { buffer.clear(); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? "Default" : juce::String {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void chooseImpulseForSlot (int slotIndex);
    bool loadImpulseResponseForSlot (int slotIndex, const juce::File& file);
    juce::String getImpulseAsset (int slotIndex) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleAsyncUpdate() override;
    void loadBuiltInImpulse (int slotIndex);
    juce::AudioBuffer<float> prepareImpulse (juce::AudioFormatReader& reader) const;
    juce::dsp::Convolution& convolutionForSlot (int slotIndex);

    juce::AudioProcessorValueTreeState parameters;
    juce::dsp::Convolution convolutionA, convolutionB;
    juce::AudioBuffer<float> convolutionScratchA, convolutionScratchB, dryScratch;
    std::array<std::vector<float>, 2> predelayLines;
    std::array<float, 2> lowpassState {}, highpassState {};
    std::size_t predelayWrite = 0;
    int maximumBlockSize = 1;
    double sampleRate = 48000.0, modulationPhase = 0.0;
    std::atomic<bool> prepared { false };

    mutable juce::CriticalSection assetLock;
    std::array<juce::String, 2> assetPaths { "built-in:reference-a", "built-in:reference-b" };
    std::array<juce::String, 2> assetStatus { "Built-in A", "Built-in B" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionLabAudioProcessor)
};
