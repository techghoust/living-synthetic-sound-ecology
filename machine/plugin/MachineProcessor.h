#pragma once

#include "machine/dsp/MachineTiming.h"
#include "machine/dsp/MechanismEngine.h"
#include "shared/audio/UserSampleSlot.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

class MachineAudioProcessor final : public juce::AudioProcessor
{
public:
    MachineAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? "Default" : juce::String {};
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    int getNumFactoryPresets() const noexcept;
    int getCurrentFactoryPreset() const noexcept;
    void applyFactoryPreset (int);
    const juce::String getFactoryPresetName (int) const;
    machine::PatternSnapshot getPatternForUi() const;
    bool submitPatternFromUi (machine::PatternSnapshot);
    void chooseLayerSample();
    bool loadLayerSample (const juce::File&);
    void clearLayerSample();
    int getCurrentStepForUi() const noexcept
    {
        return currentStepForUi.load (std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState& getParameterState() noexcept { return parameters; }
    const machine::PatternSnapshot& getPatternForTests() const noexcept { return activePattern; }
    machine::EventBuffer getLastEventsForTests() const noexcept { return lastEvents; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    machine::HostPosition readHostPosition() const noexcept;
    void restorePattern (const juce::ValueTree&) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* syncParameter = nullptr;
    std::atomic<float>* rateParameter = nullptr;
    std::atomic<float>* stepsParameter = nullptr;
    std::atomic<float>* swingParameter = nullptr;
    std::atomic<float>* probabilityParameter = nullptr;
    std::atomic<float>* ratchetParameter = nullptr;
    std::atomic<float>* seedParameter = nullptr;
    std::atomic<float>* modeParameter = nullptr;
    std::atomic<float>* forceParameter = nullptr;
    std::atomic<float>* motorParameter = nullptr;
    std::atomic<float>* relayParameter = nullptr;
    std::atomic<float>* frictionParameter = nullptr;
    std::atomic<float>* bodyParameter = nullptr;
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* driveParameter = nullptr;
    std::atomic<float>* toneParameter = nullptr;
    std::atomic<float>* irregularityParameter = nullptr;
    std::atomic<float>* stereoParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;

    machine::TransportClock transportClock;
    machine::MechanismEngine mechanismEngine;
    machine::PatternSnapshot activePattern;
    machine::PatternSnapshot uiPattern;
    machine::PatternSnapshotQueue pendingPatterns;
    machine::EventBuffer lastEvents;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> forceAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> motorAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> relayAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frictionAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bodyAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> irregularityAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> stereoAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    float excitationEnvelope = 0.0f;
    float excitationAttack = 0.0f;
    float excitationRelease = 0.0f;
    mutable juce::CriticalSection uiPatternLock;
    std::atomic<int> currentFactoryPreset { 0 };
    std::atomic<int> currentStepForUi { -1 };
    lsse::audio::UserSampleSlot layerSample;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String layerSamplePath;
    juce::CriticalSection layerSamplePathLock;
    double layerSamplePosition = 0.0;
    bool layerSampleActive = false;
    double processingSampleRate = 48000.0;
    std::atomic<bool> layerSampleResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MachineAudioProcessor)
};
