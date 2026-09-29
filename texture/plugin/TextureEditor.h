#pragma once

#include "TextureProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

class TextureLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TextureLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                           float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
};

class TextureParameterControl final : public juce::Component
{
public:
    TextureParameterControl (juce::AudioProcessorValueTreeState&, const juce::String& id,
                             const juce::String& title, const juce::String& tooltip,
                             TextureLookAndFeel&, bool horizontal = false);
    ~TextureParameterControl() override;
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    bool isHorizontal = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class TextureSurfaceDisplay final : public juce::Component
{
public:
    explicit TextureSurfaceDisplay (TextureAudioProcessor& owner) : processor (owner) {}
    void paint (juce::Graphics&) override;
    void setVisualPhase (float newPhase) noexcept { phase = newPhase; }

private:
    TextureAudioProcessor& processor;
    float phase = 0.0f;
};

class TextureAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TextureAudioProcessorEditor (TextureAudioProcessor&);
    ~TextureAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutFour (juce::Rectangle<int>, const std::array<juce::Component*, 4>&);

    TextureAudioProcessor& processor;
    TextureLookAndFeel lookAndFeel;
    TextureSurfaceDisplay surface;
    TextureParameterControl capture;
    TextureParameterControl grainSize;
    TextureParameterControl density;
    TextureParameterControl spray;
    TextureParameterControl pitch;
    TextureParameterControl reverse;
    TextureParameterControl blur;
    TextureParameterControl detail;
    TextureParameterControl wear;
    TextureParameterControl dropout;
    TextureParameterControl tone;
    TextureParameterControl motionRate;
    TextureParameterControl motionDepth;
    TextureParameterControl width;
    TextureParameterControl seed;
    TextureParameterControl input;
    TextureParameterControl mix;
    TextureParameterControl output;
    juce::ToggleButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    juce::ToggleButton reduceMotionButton { "REDUCE MOTION" };
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TooltipWindow tooltipWindow { this, 650 };
    std::array<juce::Rectangle<int>, 4> groupBounds {};
    float visualPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextureAudioProcessorEditor)
};
