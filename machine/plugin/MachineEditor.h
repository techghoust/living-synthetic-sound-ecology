#pragma once

#include "MachineProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

class MachineLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MachineLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                           float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
};

class MachineParameterControl final : public juce::Component
{
public:
    MachineParameterControl (juce::AudioProcessorValueTreeState&, const juce::String& id,
                             const juce::String& title, const juce::String& tooltip,
                             MachineLookAndFeel&, bool horizontal = false);
    ~MachineParameterControl() override;
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    bool isHorizontal = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class MachineStepRail final : public juce::Component
{
public:
    enum class EditMode { value, accent, probability, ratchet };

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setPattern (const machine::PatternSnapshot&);
    const machine::PatternSnapshot& getPattern() const noexcept { return pattern; }
    void setEditMode (EditMode newMode) noexcept { mode = newMode; repaint(); }
    void setCurrentStep (int step) noexcept;
    bool isEditing() const noexcept { return editing; }
    std::function<void(const machine::PatternSnapshot&)> onPatternCommitted;

private:
    void updateFromPosition (juce::Point<float>);
    float displayedValue (const machine::StepState&) const noexcept;

    machine::PatternSnapshot pattern;
    EditMode mode = EditMode::value;
    int currentStep = -1;
    int editedStep = -1;
    bool editing = false;
};

class MachineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit MachineAudioProcessorEditor (MachineAudioProcessor&);
    ~MachineAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void commitPattern (machine::PatternSnapshot);
    void randomizePattern();
    void clearPattern();
    void layoutControls (juce::Rectangle<int>, const std::array<juce::Component*, 4>&);

    MachineAudioProcessor& processor;
    MachineLookAndFeel lookAndFeel;
    MachineStepRail stepRail;

    MachineParameterControl force;
    MachineParameterControl motor;
    MachineParameterControl relay;
    MachineParameterControl friction;
    MachineParameterControl steps;
    MachineParameterControl swing;
    MachineParameterControl probability;
    MachineParameterControl ratchet;
    MachineParameterControl body;
    MachineParameterControl feedback;
    MachineParameterControl drive;
    MachineParameterControl tone;
    MachineParameterControl irregularity;
    MachineParameterControl stereo;
    MachineParameterControl seed;
    MachineParameterControl mix;
    MachineParameterControl output;
    MachineParameterControl sampleBlend;
    MachineParameterControl samplePitch;

    juce::Label modeLabel;
    juce::ComboBox modeBox;
    juce::Label rateLabel;
    juce::ComboBox rateBox;
    juce::ToggleButton syncButton { "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    juce::Label sampleLayerLabel;
    juce::ComboBox sampleLayerBox;
    juce::ToggleButton sampleLoopButton { "LOOP" };
    juce::TextButton loadSampleButton { "LOAD SAMPLE" };
    juce::TextButton clearSampleButton { "CLEAR SAMPLE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sampleLayerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sampleLoopAttachment;

    juce::Label presetLabel;
    juce::ComboBox presetBox;
    std::array<juce::TextButton, 4> editButtons {
        juce::TextButton { "VALUE" }, juce::TextButton { "ACCENT" },
        juce::TextButton { "PROB" }, juce::TextButton { "RATCHET" }
    };
    juce::TextButton copyButton { "COPY" };
    juce::TextButton pasteButton { "PASTE" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton randomButton { "RANDOMIZE" };
    machine::PatternSnapshot copiedPattern;
    bool hasCopiedPattern = false;
    bool clearArmed = false;
    double clearDeadlineMs = 0.0;
    std::array<juce::Rectangle<int>, 3> groupBounds {};
    juce::TooltipWindow tooltipWindow { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MachineAudioProcessorEditor)
};
