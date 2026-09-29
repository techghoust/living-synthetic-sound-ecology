#pragma once

#include "texture/dsp/CaptureBuffer.h"
#include "texture/dsp/DetailWear.h"
#include "texture/dsp/GrainEngine.h"
#include "texture/dsp/SpectralSurface.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class TextureAudioProcessor final : public juce::AudioProcessor
{
public:
    struct VisualizationSnapshot
    {
        float inputLevel = 0.0f;
        float outputLevel = 0.0f;
        float surfaceEnergy = 0.0f;
        std::size_t activeGrains = 0;
        bool frozen = false;
    };

    TextureAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    bool supportsDoublePrecisionProcessing() const override { return false; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    int getNumFactoryPresets() const noexcept;
    int getCurrentFactoryPreset() const noexcept;
    void applyFactoryPreset (int);
    const juce::String getFactoryPresetName (int) const;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorValueTreeState& getParameterState() noexcept { return parameters; }
    [[nodiscard]] VisualizationSnapshot getVisualizationSnapshot() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateGrainSettings() noexcept;

    static constexpr double captureSeconds = 3.0;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* inputParameter = nullptr;
    std::atomic<float>* captureParameter = nullptr;
    std::atomic<float>* grainSizeParameter = nullptr;
    std::atomic<float>* densityParameter = nullptr;
    std::atomic<float>* sprayParameter = nullptr;
    std::atomic<float>* pitchParameter = nullptr;
    std::atomic<float>* reverseParameter = nullptr;
    std::atomic<float>* blurParameter = nullptr;
    std::atomic<float>* freezeParameter = nullptr;
    std::atomic<float>* detailParameter = nullptr;
    std::atomic<float>* wearParameter = nullptr;
    std::atomic<float>* dropoutParameter = nullptr;
    std::atomic<float>* motionRateParameter = nullptr;
    std::atomic<float>* motionDepthParameter = nullptr;
    std::atomic<float>* toneParameter = nullptr;
    std::atomic<float>* widthParameter = nullptr;
    std::atomic<float>* seedParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;

    texture::CaptureBuffer captureBuffer;
    texture::GrainEngine grainEngine;
    texture::SpectralSurface spectralSurface;
    texture::DetailWear detailWear;
    std::array<std::array<float, texture::SpectralSurface::fftSize + 1>, 2> dryDelay {};
    std::size_t dryDelayPosition = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    std::uint32_t activeSeed = 1977;
    std::atomic<float> visualizationInput { 0.0f };
    std::atomic<float> visualizationOutput { 0.0f };
    std::atomic<float> visualizationSurface { 0.0f };
    std::atomic<std::size_t> visualizationGrains { 0 };
    std::atomic<bool> visualizationFrozen { false };
    std::atomic<int> currentFactoryPreset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextureAudioProcessor)
};
