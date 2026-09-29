#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

class MemoryLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MemoryLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                           float, float, juce::Slider&) override;
};

class ParameterControl final : public juce::Component
{
public:
    ParameterControl (juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
                      const juce::String& title, MemoryLookAndFeel&);
    ~ParameterControl() override;
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class HistoryTimeline final : public juce::Component
{
public:
    explicit HistoryTimeline (MemoryAudioProcessor& owner) : processor (owner) {}
    void paint (juce::Graphics&) override;

private:
    MemoryAudioProcessor& processor;
};

class MemoryAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit MemoryAudioProcessorEditor (MemoryAudioProcessor&);
    ~MemoryAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MemoryAudioProcessor& processor;
    MemoryLookAndFeel lookAndFeel;
    HistoryTimeline timeline;
    ParameterControl memoryLength;
    ParameterControl age;
    ParameterControl recall;
    ParameterControl fragment;
    ParameterControl decay;
    ParameterControl corruption;
    ParameterControl drift;
    ParameterControl repeat;
    ParameterControl feedback;
    juce::Label mixLabel;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    juce::Label outputLabel;
    juce::Slider outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    juce::Label presetLabel;
    juce::ComboBox presetBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoryAudioProcessorEditor)
};
