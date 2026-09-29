#pragma once

#include "shared/AudioHistory.h"
#include "shared/RecallEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class MemoryAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr std::size_t visualizationBinCount = 128;

    struct VisualizationSnapshot
    {
        std::array<float, visualizationBinCount> activity {};
        std::size_t writeIndex = 0;
        std::size_t activeEvents = 0;
        float recalledAge = 0.0f;
    };

    MemoryAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool supportsDoublePrecisionProcessing() const override { return false; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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
    void updateRecallSettings() noexcept;
    void updateMixTargets() noexcept;
    void publishInputActivity (const juce::AudioBuffer<float>&) noexcept;

    static constexpr double maximumHistorySeconds = 120.0;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* memoryLengthParameter = nullptr;
    std::atomic<float>* recallParameter = nullptr;
    std::atomic<float>* ageParameter = nullptr;
    std::atomic<float>* fragmentParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* corruptionParameter = nullptr;
    std::atomic<float>* driftParameter = nullptr;
    std::atomic<float>* repeatParameter = nullptr;
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* pastPresentParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;

    memory::AudioHistory audioHistory;
    memory::RecallEngine recallEngine;
    juce::AudioBuffer<float> recallScratch;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> presentGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pastGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;

    std::array<std::atomic<float>, visualizationBinCount> visualizationActivity {};
    std::atomic<std::size_t> visualizationWriteIndex { 0 };
    std::atomic<std::size_t> visualizationActiveEvents { 0 };
    std::atomic<float> visualizationRecalledAge { 0.0f };
    std::atomic<int> currentFactoryPreset { 0 };
    std::size_t visualizationFramesPerBin = 1;
    std::size_t visualizationFramesAccumulated = 0;
    float visualizationPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoryAudioProcessor)
};
