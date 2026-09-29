#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "environment/dsp/BedScheduler.h"
#include <cstdint>
#include <array>

class EnvironmentAudioProcessor final : public juce::AudioProcessor
{
public:
    EnvironmentAudioProcessor();
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
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    lsse::environment::BedScheduler scheduler;
    std::uint64_t timeline = 0;
    std::array<float, 2> spaceState {};
    std::array<float, 2> toneState {};
};
