#include "MachineEditor.h"

#include <algorithm>
#include <cmath>

namespace machinePalette
{
const auto background = juce::Colour::fromRGB (12, 15, 17);
const auto panel = juce::Colour::fromRGB (24, 29, 31);
const auto raised = juce::Colour::fromRGB (35, 42, 44);
const auto edge = juce::Colour::fromRGB (62, 72, 73);
const auto text = juce::Colour::fromRGB (231, 233, 224);
const auto muted = juce::Colour::fromRGB (139, 148, 143);
const auto amber = juce::Colour::fromRGB (239, 166, 54);
const auto green = juce::Colour::fromRGB (111, 205, 132);
const auto red = juce::Colour::fromRGB (224, 83, 70);
}

MachineLookAndFeel::MachineLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, machinePalette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, machinePalette::panel);
    setColour (juce::ComboBox::textColourId, machinePalette::text);
    setColour (juce::ComboBox::outlineColourId, machinePalette::edge);
    setColour (juce::ComboBox::arrowColourId, machinePalette::amber);
    setColour (juce::PopupMenu::backgroundColourId, machinePalette::panel);
    setColour (juce::PopupMenu::textColourId, machinePalette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               machinePalette::amber.withAlpha (0.35f));
    setColour (juce::TextButton::textColourOffId, machinePalette::muted);
    setColour (juce::TextButton::textColourOnId, machinePalette::text);
}

void MachineLookAndFeel::drawRotarySlider (juce::Graphics& graphics, const int x, const int y,
                                            const int width, const int height,
                                            const float position, const float startAngle,
                                            const float endAngle, juce::Slider& slider)
{
    const auto diameter = static_cast<float> (std::min (width, height)) - 10.0f;
    const auto bounds = juce::Rectangle<float> (
        static_cast<float> (x) + (static_cast<float> (width) - diameter) * 0.5f,
        static_cast<float> (y) + 4.0f, diameter, diameter);
    const auto thickness = std::max (2.0f, diameter * 0.075f);
    const auto angle = startAngle + position * (endAngle - startAngle);
    graphics.setColour (machinePalette::edge);
    graphics.drawEllipse (bounds.reduced (thickness * 0.5f), thickness);
    juce::Path arc;
    arc.addCentredArc (bounds.getCentreX(), bounds.getCentreY(),
                       bounds.getWidth() * 0.5f - thickness,
                       bounds.getHeight() * 0.5f - thickness,
                       0.0f, startAngle, angle, true);
    graphics.setColour (machinePalette::amber);
    graphics.strokePath (arc, juce::PathStrokeType (thickness,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto centre = bounds.getCentre();
    const auto tip = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                  * diameter * 0.27f;
    graphics.setColour (machinePalette::text);
    graphics.drawLine ({ centre, tip }, 1.8f);
    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (machinePalette::green);
        graphics.drawRoundedRectangle (bounds.expanded (4.0f), 5.0f, 1.5f);
    }
}

void MachineLookAndFeel::drawLinearSlider (juce::Graphics& graphics, const int x, const int y,
                                            const int width, const int height,
                                            const float sliderPosition, float, float,
                                            const juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
        return LookAndFeel_V4::drawLinearSlider (graphics, x, y, width, height,
                                                  sliderPosition, 0.0f, 0.0f, style, slider);
    const auto centreY = static_cast<float> (y + height / 2);
    graphics.setColour (machinePalette::edge);
    graphics.drawLine (static_cast<float> (x), centreY,
                       static_cast<float> (x + width), centreY, 4.0f);
    graphics.setColour (machinePalette::amber);
    graphics.drawLine (static_cast<float> (x), centreY, sliderPosition, centreY, 4.0f);
    graphics.fillEllipse (sliderPosition - 5.5f, centreY - 5.5f, 11.0f, 11.0f);
}

void MachineLookAndFeel::drawButtonBackground (juce::Graphics& graphics, juce::Button& button,
                                                const juce::Colour&, const bool highlighted,
                                                const bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto colour = button.getToggleState() ? machinePalette::amber.darker (0.35f)
                                           : machinePalette::panel;
    if (button.getButtonText().containsIgnoreCase ("CONFIRM"))
        colour = machinePalette::red.darker (0.25f);
    if (highlighted)
        colour = colour.brighter (0.12f);
    if (down)
        colour = colour.darker (0.18f);
    graphics.setColour (colour);
    graphics.fillRoundedRectangle (bounds, 4.0f);
    graphics.setColour (button.hasKeyboardFocus (true) ? machinePalette::green
                                                        : machinePalette::edge);
    graphics.drawRoundedRectangle (bounds, 4.0f,
                                    button.hasKeyboardFocus (true) ? 1.8f : 1.0f);
}

MachineParameterControl::MachineParameterControl (
    juce::AudioProcessorValueTreeState& state, const juce::String& id,
    const juce::String& title, const juce::String& tooltip,
    MachineLookAndFeel& lookAndFeel, const bool horizontal)
    : isHorizontal (horizontal)
{
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, machinePalette::muted);
    label.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    addAndMakeVisible (label);
    slider.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal
                                      : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (horizontal ? juce::Slider::TextBoxRight
                                       : juce::Slider::TextBoxBelow,
                            false, horizontal ? 64 : 70, 18);
    slider.setLookAndFeel (&lookAndFeel);
    slider.setTitle (title);
    slider.setDescription (tooltip);
    slider.setTooltip (tooltip);
    slider.setWantsKeyboardFocus (true);
    if (const auto* parameter = state.getParameter (id))
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, id, slider);
}

MachineParameterControl::~MachineParameterControl()
{
    slider.setLookAndFeel (nullptr);
}

void MachineParameterControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (17));
    slider.setBounds (isHorizontal ? area.reduced (4, 3) : area);
}

float MachineStepRail::displayedValue (const machine::StepState& step) const noexcept
{
    switch (mode)
    {
        case EditMode::value: return step.value;
        case EditMode::accent: return step.accent;
        case EditMode::probability: return step.probability;
        case EditMode::ratchet: return static_cast<float> (step.ratchet - 1U) / 3.0f;
    }
    return 0.0f;
}

void MachineStepRail::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (machinePalette::panel);
    graphics.fillRoundedRectangle (bounds, 8.0f);
    graphics.setColour (machinePalette::edge);
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    auto plot = bounds.reduced (16.0f, 12.0f);
    plot.removeFromTop (18.0f);
    const auto stepWidth = plot.getWidth() / static_cast<float> (machine::maxSteps);
    graphics.setColour (machinePalette::edge.withAlpha (0.35f));
    for (int row = 1; row < 4; ++row)
        graphics.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * row / 4.0f),
                                     plot.getX(), plot.getRight());

    for (std::size_t index = 0; index < pattern.steps.size(); ++index)
    {
        const auto x = plot.getX() + static_cast<float> (index) * stepWidth;
        auto cell = juce::Rectangle<float> (x + 2.0f, plot.getY(), stepWidth - 4.0f,
                                            plot.getHeight());
        const auto active = index < pattern.activeSteps;
        const auto amount = std::clamp (displayedValue (pattern.steps[index]), 0.0f, 1.0f);
        graphics.setColour ((active ? machinePalette::raised : machinePalette::background)
                                .withAlpha (active ? 1.0f : 0.55f));
        graphics.fillRoundedRectangle (cell, 3.0f);
        auto bar = cell.reduced (3.0f);
        bar.removeFromTop (bar.getHeight() * (1.0f - amount));
        graphics.setColour ((mode == EditMode::accent ? machinePalette::red
                              : mode == EditMode::probability ? machinePalette::green
                                                             : machinePalette::amber)
                                .withAlpha (active ? 0.9f : 0.25f));
        graphics.fillRoundedRectangle (bar, 2.0f);
        if (pattern.steps[index].accent > 0.65f)
        {
            graphics.setColour (machinePalette::red);
            graphics.fillEllipse (cell.getCentreX() - 2.5f, cell.getY() + 4.0f, 5.0f, 5.0f);
        }
        if (currentStep == static_cast<int> (index))
        {
            graphics.setColour (machinePalette::green);
            graphics.drawRoundedRectangle (cell.expanded (1.0f), 4.0f, 2.0f);
        }
        graphics.setColour (active ? machinePalette::text : machinePalette::muted.withAlpha (0.45f));
        graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        graphics.drawText (juce::String (index + 1), juce::roundToInt (cell.getX()),
                           juce::roundToInt (bounds.getY() + 3.0f),
                           juce::roundToInt (cell.getWidth()), 15,
                           juce::Justification::centred);
        if (pattern.steps[index].ratchet > 1)
            graphics.drawText ("x" + juce::String (pattern.steps[index].ratchet),
                               cell.toNearestInt().removeFromBottom (15),
                               juce::Justification::centred);
    }
}

void MachineStepRail::setPattern (const machine::PatternSnapshot& newPattern)
{
    pattern = newPattern;
    repaint();
}

void MachineStepRail::setCurrentStep (const int step) noexcept
{
    if (currentStep != step)
    {
        currentStep = step;
        repaint();
    }
}

void MachineStepRail::updateFromPosition (const juce::Point<float> position)
{
    auto plot = getLocalBounds().toFloat().reduced (16.0f, 12.0f);
    plot.removeFromTop (18.0f);
    if (! plot.contains (position))
        return;
    const auto stepWidth = plot.getWidth() / static_cast<float> (machine::maxSteps);
    const auto index = std::clamp (static_cast<int> ((position.x - plot.getX()) / stepWidth),
                                   0, static_cast<int> (machine::maxSteps - 1));
    const auto amount = std::clamp (1.0f - (position.y - plot.getY()) / plot.getHeight(),
                                    0.0f, 1.0f);
    auto& step = pattern.steps[static_cast<std::size_t> (index)];
    switch (mode)
    {
        case EditMode::value: step.value = amount; break;
        case EditMode::accent: step.accent = amount; break;
        case EditMode::probability: step.probability = amount; break;
        case EditMode::ratchet:
            step.ratchet = static_cast<std::uint8_t> (
                std::clamp (1 + juce::roundToInt (amount * 3.0f), 1, 4));
            break;
    }
    editedStep = index;
    repaint();
}

void MachineStepRail::mouseDown (const juce::MouseEvent& event)
{
    editing = true;
    updateFromPosition (event.position);
}

void MachineStepRail::mouseDrag (const juce::MouseEvent& event)
{
    updateFromPosition (event.position);
}

void MachineStepRail::mouseUp (const juce::MouseEvent&)
{
    editing = false;
    if (editedStep >= 0 && onPatternCommitted)
    {
        ++pattern.revision;
        onPatternCommitted (pattern);
    }
    editedStep = -1;
}

MachineAudioProcessorEditor::MachineAudioProcessorEditor (MachineAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner),
      force (owner.getParameterState(), "force", "FORCE", "Mechanical strike intensity", lookAndFeel),
      motor (owner.getParameterState(), "motor", "MOTOR", "Continuous motor voice", lookAndFeel),
      relay (owner.getParameterState(), "relay", "RELAY", "Relay and switch impulses", lookAndFeel),
      friction (owner.getParameterState(), "friction", "FRICTION", "Input-following mechanical friction", lookAndFeel),
      steps (owner.getParameterState(), "steps", "STEPS", "Active pattern length", lookAndFeel),
      swing (owner.getParameterState(), "swing", "SWING", "Alternating step displacement", lookAndFeel),
      probability (owner.getParameterState(), "probability", "PROBABILITY", "Global event probability", lookAndFeel),
      ratchet (owner.getParameterState(), "ratchet", "RATCHET", "Global repeat count per step", lookAndFeel),
      body (owner.getParameterState(), "body", "BODY", "Resonant mechanical enclosure", lookAndFeel),
      feedback (owner.getParameterState(), "feedback", "FEEDBACK", "Bounded short mechanical echo", lookAndFeel),
      drive (owner.getParameterState(), "drive", "DRIVE", "Oversampled saturation", lookAndFeel),
      tone (owner.getParameterState(), "tone", "TONE", "Dark to bright mechanism colour", lookAndFeel),
      irregularity (owner.getParameterState(), "irregularity", "IRREGULAR", "Seeded motion instability", lookAndFeel),
      stereo (owner.getParameterState(), "stereo", "STEREO", "Stereo mechanical spread", lookAndFeel),
      seed (owner.getParameterState(), "seed", "SEED", "Repeatable variation seed", lookAndFeel),
      mix (owner.getParameterState(), "mix", "MIX", "Equal-power dry and mechanism blend", lookAndFeel),
      output (owner.getParameterState(), "output", "OUTPUT", "Final output trim", lookAndFeel),
      sampleBlend (owner.getParameterState(), "sample_blend", "SAMPLE BLEND", "Procedural and loaded layer balance", lookAndFeel),
      samplePitch (owner.getParameterState(), "sample_pitch", "SAMPLE PITCH", "Loaded layer tuning", lookAndFeel)
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &stepRail, &force, &motor, &relay, &friction, &steps, &swing,
             &probability, &ratchet, &body, &feedback, &drive, &tone,
             &irregularity, &stereo, &seed, &mix, &output,
             &modeLabel, &modeBox, &rateLabel, &rateBox, &syncButton,
             &presetLabel, &presetBox, &copyButton, &pasteButton,
             &clearButton, &randomButton, &sampleBlend, &samplePitch,
             &sampleLayerLabel, &sampleLayerBox, &sampleLoopButton,
             &loadSampleButton, &clearSampleButton })
        addAndMakeVisible (component);
    for (auto& button : editButtons)
        addAndMakeVisible (button);

    modeLabel.setText ("MODE", juce::dontSendNotification);
    rateLabel.setText ("RATE", juce::dontSendNotification);
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    for (auto* label : { &modeLabel, &rateLabel, &presetLabel })
    {
        label->setColour (juce::Label::textColourId, machinePalette::muted);
        label->setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        label->setJustificationType (juce::Justification::centredRight);
    }
    modeBox.addItemList ({ "HYBRID", "MOTOR", "RELAY" }, 1);
    rateBox.addItemList ({ "1/32 / 16 Hz", "1/16T / 12 Hz", "1/16 / 8 Hz",
                           "1/8T / 6 Hz", "1/8 / 4 Hz", "1/4T / 3 Hz", "1/4 / 2 Hz" }, 1);
    for (auto* box : { &modeBox, &rateBox, &presetBox })
    {
        box->setLookAndFeel (&lookAndFeel);
        box->setWantsKeyboardFocus (true);
    }
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        owner.getParameterState(), "mode", modeBox);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        owner.getParameterState(), "rate", rateBox);
    syncButton.setLookAndFeel (&lookAndFeel);
    syncButton.setClickingTogglesState (true);
    syncButton.setTooltip ("Lock the sequencer to FL Studio transport");
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        owner.getParameterState(), "sync", syncButton);
    sampleLayerLabel.setText ("SAMPLE LAYER", juce::dontSendNotification);
    sampleLayerLabel.setColour (juce::Label::textColourId, machinePalette::muted);
    sampleLayerLabel.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    sampleLayerBox.addItemList ({ "ACTUATOR", "MOTOR", "FRICTION", "BODY", "IMPACT",
                                  "ELECTRICAL", "RESONANCE" }, 1);
    sampleLayerBox.setLookAndFeel (&lookAndFeel);
    sampleLayerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        owner.getParameterState(), "sample_layer", sampleLayerBox);
    sampleLoopButton.setLookAndFeel (&lookAndFeel);
    sampleLoopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        owner.getParameterState(), "sample_loop", sampleLoopButton);
    for (auto* button : { &loadSampleButton, &clearSampleButton })
        button->setLookAndFeel (&lookAndFeel);
    loadSampleButton.onClick = [this] { processor.chooseLayerSample(); };
    clearSampleButton.onClick = [this] { processor.clearLayerSample(); };

    for (int index = 0; index < owner.getNumFactoryPresets(); ++index)
        presetBox.addItem (owner.getFactoryPresetName (index), index + 1);
    presetBox.setSelectedId (owner.getCurrentFactoryPreset() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.applyFactoryPreset (presetBox.getSelectedId() - 1);
        stepRail.setPattern (processor.getPatternForUi());
    };

    constexpr std::array<MachineStepRail::EditMode, 4> modes {
        MachineStepRail::EditMode::value, MachineStepRail::EditMode::accent,
        MachineStepRail::EditMode::probability, MachineStepRail::EditMode::ratchet
    };
    for (std::size_t index = 0; index < editButtons.size(); ++index)
    {
        auto& button = editButtons[index];
        button.setLookAndFeel (&lookAndFeel);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (731);
        button.onClick = [this, index, modes] { stepRail.setEditMode (modes[index]); };
    }
    editButtons.front().setToggleState (true, juce::dontSendNotification);
    for (auto* button : { &copyButton, &pasteButton, &clearButton, &randomButton })
        button->setLookAndFeel (&lookAndFeel);
    copyButton.onClick = [this]
    {
        copiedPattern = stepRail.getPattern();
        hasCopiedPattern = true;
        pasteButton.setEnabled (true);
    };
    pasteButton.setEnabled (false);
    pasteButton.onClick = [this]
    {
        if (hasCopiedPattern)
            commitPattern (copiedPattern);
    };
    clearButton.onClick = [this]
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (! clearArmed || now > clearDeadlineMs)
        {
            clearArmed = true;
            clearDeadlineMs = now + 2500.0;
            clearButton.setButtonText ("CONFIRM CLEAR");
            return;
        }
        clearPattern();
        clearArmed = false;
        clearButton.setButtonText ("CLEAR");
    };
    randomButton.onClick = [this] { randomizePattern(); };
    stepRail.onPatternCommitted = [this] (const auto& pattern) { commitPattern (pattern); };
    stepRail.setPattern (processor.getPatternForUi());

    setSize (1120, 760);
    setResizable (true, true);
    setResizeLimits (900, 680, 1500, 1000);
    startTimerHz (30);
}

MachineAudioProcessorEditor::~MachineAudioProcessorEditor()
{
    stopTimer();
    for (auto* component : std::initializer_list<juce::Component*> {
             &modeBox, &rateBox, &presetBox, &syncButton, &copyButton,
             &pasteButton, &clearButton, &randomButton, &sampleLayerBox,
             &sampleLoopButton, &loadSampleButton, &clearSampleButton })
        component->setLookAndFeel (nullptr);
    for (auto& button : editButtons)
        button.setLookAndFeel (nullptr);
}

void MachineAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (machinePalette::background);
    graphics.setColour (machinePalette::text);
    graphics.setFont (juce::FontOptions { 28.0f, juce::Font::bold });
    graphics.drawText ("MACHINE", 26, 13, 190, 34, juce::Justification::centredLeft);
    graphics.setColour (machinePalette::amber);
    graphics.fillRect (27, 53, 67, 2);
    graphics.setColour (machinePalette::muted);
    graphics.setFont (juce::FontOptions { 10.5f });
    graphics.drawText ("LIVING SYNTHETIC SOUND ECOLOGY", 108, 41, 250, 18,
                       juce::Justification::centredLeft);
    constexpr std::array<const char*, 3> titles { "MECHANISM", "MOTION", "BODY / COLOUR" };
    for (std::size_t index = 0; index < groupBounds.size(); ++index)
    {
        const auto bounds = groupBounds[index].toFloat();
        graphics.setColour (machinePalette::panel);
        graphics.fillRoundedRectangle (bounds, 7.0f);
        graphics.setColour (machinePalette::edge);
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);
        graphics.setColour (index == 1 ? machinePalette::green : machinePalette::amber);
        graphics.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        graphics.drawText (titles[index], groupBounds[index].reduced (11, 5).removeFromTop (15),
                           juce::Justification::centredLeft);
    }
}

void MachineAudioProcessorEditor::layoutControls (
    juce::Rectangle<int> area, const std::array<juce::Component*, 4>& controls)
{
    area.removeFromTop (18);
    const auto width = area.getWidth() / 4;
    for (std::size_t index = 0; index < controls.size(); ++index)
        controls[index]->setBounds (area.removeFromLeft (
            index == controls.size() - 1 ? area.getWidth() : width).reduced (3));
}

void MachineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (26);
    auto header = area.removeFromTop (42);
    presetBox.setBounds (header.removeFromRight (235).reduced (0, 4));
    presetLabel.setBounds (header.removeFromRight (60));

    auto transport = area.removeFromTop (42);
    modeLabel.setBounds (transport.removeFromLeft (48));
    modeBox.setBounds (transport.removeFromLeft (132).reduced (2, 5));
    transport.removeFromLeft (10);
    rateLabel.setBounds (transport.removeFromLeft (42));
    rateBox.setBounds (transport.removeFromLeft (154).reduced (2, 5));
    transport.removeFromLeft (9);
    syncButton.setBounds (transport.removeFromLeft (78).reduced (2, 5));

    auto source = area.removeFromTop (40);
    sampleLayerLabel.setBounds (source.removeFromLeft (82));
    sampleLayerBox.setBounds (source.removeFromLeft (132).reduced (2, 5));
    sampleLoopButton.setBounds (source.removeFromLeft (74).reduced (2, 5));
    loadSampleButton.setBounds (source.removeFromLeft (112).reduced (3, 5));
    clearSampleButton.setBounds (source.removeFromLeft (118).reduced (3, 5));

    auto railArea = area.removeFromTop (190);
    auto buttons = railArea.removeFromTop (32);
    for (auto& button : editButtons)
        button.setBounds (buttons.removeFromLeft (82).reduced (2));
    buttons.removeFromLeft (12);
    randomButton.setBounds (buttons.removeFromRight (112).reduced (2));
    clearButton.setBounds (buttons.removeFromRight (116).reduced (2));
    pasteButton.setBounds (buttons.removeFromRight (72).reduced (2));
    copyButton.setBounds (buttons.removeFromRight (72).reduced (2));
    stepRail.setBounds (railArea.reduced (0, 4));
    area.removeFromTop (8);

    auto groups = area.removeFromTop (230);
    const auto groupWidth = groups.getWidth() / 3;
    for (std::size_t index = 0; index < groupBounds.size(); ++index)
        groupBounds[index] = groups.removeFromLeft (
            index == groupBounds.size() - 1 ? groups.getWidth() : groupWidth).reduced (4, 0);
    layoutControls (groupBounds[0], { &force, &motor, &relay, &friction });
    layoutControls (groupBounds[1], { &steps, &swing, &probability, &ratchet });
    layoutControls (groupBounds[2], { &body, &feedback, &drive, &tone });

    area.removeFromTop (8);
    const std::array<juce::Component*, 7> globals {
        &irregularity, &stereo, &seed, &sampleBlend, &samplePitch, &mix, &output
    };
    const auto globalWidth = area.getWidth() / static_cast<int> (globals.size());
    for (std::size_t index = 0; index < globals.size(); ++index)
        globals[index]->setBounds (area.removeFromLeft (
            index == globals.size() - 1 ? area.getWidth() : globalWidth).reduced (4));
}

void MachineAudioProcessorEditor::commitPattern (machine::PatternSnapshot pattern)
{
    pattern.revision = std::max (pattern.revision + 1U, 1U);
    if (const auto* value = processor.getParameterState().getRawParameterValue ("steps"))
        pattern.activeSteps = static_cast<std::uint8_t> (
            std::clamp (juce::roundToInt (value->load()), 1, 16));
    if (const auto* value = processor.getParameterState().getRawParameterValue ("seed"))
        pattern.seed = static_cast<std::uint32_t> (
            std::clamp (juce::roundToInt (value->load()), 1, 65535));
    if (processor.submitPatternFromUi (pattern))
        stepRail.setPattern (pattern);
}

void MachineAudioProcessorEditor::randomizePattern()
{
    auto pattern = stepRail.getPattern();
    for (std::size_t index = 0; index < pattern.steps.size(); ++index)
    {
        const auto key = machine::TransportClock::makeRandomKey (
            pattern.seed, pattern.revision + 1U, 0, static_cast<std::uint8_t> (index), 0, 29);
        const auto a = static_cast<float> (machine::TransportClock::randomUnit (key));
        const auto b = static_cast<float> (machine::TransportClock::randomUnit (key ^ 0x9e3779b97f4a7c15ULL));
        auto& step = pattern.steps[index];
        step.value = 0.2f + 0.8f * a;
        step.accent = b > 0.72f ? 1.0f : 0.15f + 0.35f * b;
        step.probability = 0.55f + 0.45f * b;
        step.ratchet = static_cast<std::uint8_t> (1 + std::min (3, static_cast<int> (a * 4.0f)));
    }
    commitPattern (pattern);
}

void MachineAudioProcessorEditor::clearPattern()
{
    auto pattern = stepRail.getPattern();
    for (auto& step : pattern.steps)
    {
        step.value = 0.0f;
        step.accent = 0.0f;
        step.probability = 0.0f;
        step.ratchet = 1;
    }
    commitPattern (pattern);
}

void MachineAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (clearArmed && now > clearDeadlineMs)
    {
        clearArmed = false;
        clearButton.setButtonText ("CLEAR");
    }
    if (! stepRail.isEditing())
    {
        auto pattern = processor.getPatternForUi();
        if (const auto* value = processor.getParameterState().getRawParameterValue ("steps"))
            pattern.activeSteps = static_cast<std::uint8_t> (
                std::clamp (juce::roundToInt (value->load()), 1, 16));
        stepRail.setPattern (pattern);
    }
    stepRail.setCurrentStep (processor.getCurrentStepForUi());
    const auto selected = processor.getCurrentFactoryPreset() + 1;
    if (presetBox.getSelectedId() != selected)
        presetBox.setSelectedId (selected, juce::dontSendNotification);
}
