#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "creature/dsp/GestureEngine.h"
#include "shared/audio/UserSampleSlot.h"
#include <array>
#include <memory>

class CreatureAudioProcessor final : public juce::AudioProcessor
{
public:
    CreatureAudioProcessor();
    void prepareToPlay (double, int) override; void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) override { b.clear(); }
    juce::AudioProcessorEditor* createEditor() override; bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; } bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; } double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; } int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {} const juce::String getProgramName (int i) override { return i == 0 ? "Default" : juce::String{}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override; void setStateInformation (const void*, int) override;
    void chooseSourceSample();
    void clearSourceSample();
    bool loadSourceSample (const juce::File&);
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    std::array<lsse::creature::GestureEngine, 2> engines;
    lsse::audio::UserSampleSlot sourceSample;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String sourcePath;
    juce::CriticalSection sourcePathLock;
    double sampleRate = 48000.0;
    double samplePosition = 0.0;
    std::atomic<bool> sourceResetRequested { false };
};
