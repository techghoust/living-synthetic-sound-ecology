#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace palette
{
const auto background = juce::Colour::fromRGB (17, 21, 27);
const auto panel = juce::Colour::fromRGB (25, 31, 39);
const auto panelEdge = juce::Colour::fromRGB (48, 59, 70);
const auto text = juce::Colour::fromRGB (225, 231, 235);
const auto muted = juce::Colour::fromRGB (126, 142, 153);
const auto memory = juce::Colour::fromRGB (70, 181, 213);
const auto recall = juce::Colour::fromRGB (239, 151, 80);
} 
MemoryLookAndFeel::MemoryLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, palette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, palette::memory);
    setColour (juce::Slider::backgroundColourId, palette::panelEdge);
    setColour (juce::ComboBox::backgroundColourId, palette::panel);
    setColour (juce::ComboBox::textColourId, palette::text);
    setColour (juce::ComboBox::outlineColourId, palette::panelEdge);
    setColour (juce::ComboBox::arrowColourId, palette::memory);
    setColour (juce::PopupMenu::backgroundColourId, palette::panel);
    setColour (juce::PopupMenu::textColourId, palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::memory.withAlpha (0.35f));
}

void MemoryLookAndFeel::drawRotarySlider (juce::Graphics& graphics, const int x, const int y,
                                          const int width, const int height,
                                          const float position, const float startAngle,
                                          const float endAngle, juce::Slider&)
{
    const auto diameter = static_cast<float> (std::min (width, height)) - 10.0f;
    const auto bounds = juce::Rectangle<float> (
        static_cast<float> (x) + (static_cast<float> (width) - diameter) * 0.5f,
        static_cast<float> (y) + 5.0f, diameter, diameter);
    const auto thickness = std::max (2.0f, diameter * 0.075f);
    const auto angle = startAngle + position * (endAngle - startAngle);

    graphics.setColour (palette::panelEdge);
    graphics.drawEllipse (bounds.reduced (thickness * 0.5f), thickness);

    juce::Path valueArc;
    valueArc.addCentredArc (bounds.getCentreX(), bounds.getCentreY(),
                            bounds.getWidth() * 0.5f - thickness,
                            bounds.getHeight() * 0.5f - thickness,
                            0.0f, startAngle, angle, true);
    graphics.setColour (palette::memory);
    graphics.strokePath (valueArc, juce::PathStrokeType (thickness,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto pointerLength = diameter * 0.28f;
    const auto centre = bounds.getCentre();
    const auto tip = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                  * pointerLength;
    graphics.setColour (palette::text);
    graphics.drawLine ({ centre, tip }, 2.0f);
}

ParameterControl::ParameterControl (juce::AudioProcessorValueTreeState& state,
                                    const juce::String& parameterId,
                                    const juce::String& title,
                                    MemoryLookAndFeel& lookAndFeel)
{
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, palette::muted);
    label.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 20);
    slider.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterId, slider);
}

ParameterControl::~ParameterControl()
{
    slider.setLookAndFeel (nullptr);
}

void ParameterControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (20));
    slider.setBounds (area);
}

void HistoryTimeline::paint (juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (palette::panel);
    graphics.fillRoundedRectangle (bounds, 10.0f);
    graphics.setColour (palette::panelEdge);
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto plot = bounds.reduced (18.0f, 30.0f);
    const auto centreY = plot.getCentreY();
    graphics.setColour (palette::panelEdge);
    graphics.drawHorizontalLine (juce::roundToInt (centreY), plot.getX(), plot.getRight());

    const auto snapshot = processor.getVisualizationSnapshot();
    juce::Path waveform;
    for (std::size_t position = 0; position < snapshot.activity.size(); ++position)
    {
        const auto source = (snapshot.writeIndex + position) % snapshot.activity.size();
        const auto level = std::sqrt (std::clamp (snapshot.activity[source], 0.0f, 1.0f));
        const auto x = plot.getX() + static_cast<float> (position)
                                     * plot.getWidth()
                                     / static_cast<float> (snapshot.activity.size() - 1);
        const auto y = centreY - level * plot.getHeight() * 0.44f;
        if (position == 0)
            waveform.startNewSubPath (x, y);
        else
            waveform.lineTo (x, y);
    }
    graphics.setColour (palette::memory.withAlpha (0.9f));
    graphics.strokePath (waveform, juce::PathStrokeType (1.8f));

    if (snapshot.activeEvents > 0)
    {
        const auto recalledX = plot.getRight() - snapshot.recalledAge * plot.getWidth();
        graphics.setColour (palette::recall.withAlpha (0.22f));
        graphics.fillRoundedRectangle (recalledX - 8.0f, plot.getY(), 16.0f,
                                       plot.getHeight(), 4.0f);
        graphics.setColour (palette::recall);
        graphics.drawVerticalLine (juce::roundToInt (recalledX), plot.getY(), plot.getBottom());
    }

    graphics.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
    graphics.setColour (palette::muted);
    graphics.drawText ("PAST", 18, 8, 80, 18, juce::Justification::centredLeft);
    graphics.drawText ("NOW", getWidth() - 98, 8, 80, 18, juce::Justification::centredRight);
    graphics.setColour (snapshot.activeEvents > 0 ? palette::recall : palette::muted);
    graphics.drawText (snapshot.activeEvents > 0 ? "RECALLING" : "LISTENING",
                       getLocalBounds().reduced (18, 8), juce::Justification::centredTop);
}

MemoryAudioProcessorEditor::MemoryAudioProcessorEditor (MemoryAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner), timeline (owner),
      memoryLength (owner.getParameterState(), "memoryLength", "MEMORY LENGTH", lookAndFeel),
      age (owner.getParameterState(), "age", "AGE", lookAndFeel),
      recall (owner.getParameterState(), "recall", "RECALL", lookAndFeel),
      fragment (owner.getParameterState(), "fragment", "FRAGMENT", lookAndFeel),
      decay (owner.getParameterState(), "decay", "DECAY", lookAndFeel),
      corruption (owner.getParameterState(), "corruption", "CORRUPTION", lookAndFeel),
      drift (owner.getParameterState(), "drift", "DRIFT", lookAndFeel),
      repeat (owner.getParameterState(), "repeat", "REPEAT", lookAndFeel),
      feedback (owner.getParameterState(), "feedback", "FEEDBACK", lookAndFeel)
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &timeline, &memoryLength, &age, &recall, &fragment, &decay,
             &corruption, &drift, &repeat, &feedback })
        addAndMakeVisible (component);

    mixLabel.setText ("PAST                              PRESENT", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setColour (juce::Label::textColourId, palette::muted);
    mixLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    addAndMakeVisible (mixLabel);

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
    mixSlider.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (mixSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        owner.getParameterState(), "pastPresent", mixSlider);

    outputLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    outputLabel.setColour (juce::Label::textColourId, palette::muted);
    outputLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    addAndMakeVisible (outputLabel);
    outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 22);
    outputSlider.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (outputSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        owner.getParameterState(), "output", outputSlider);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, palette::muted);
    presetLabel.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);

    presetBox.setLookAndFeel (&lookAndFeel);
    for (int index = 0; index < owner.getNumFactoryPresets(); ++index)
        presetBox.addItem (owner.getFactoryPresetName (index), index + 1);
    presetBox.setSelectedId (owner.getCurrentFactoryPreset() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.applyFactoryPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    setSize (780, 610);
    setResizable (true, true);
    setResizeLimits (680, 540, 1100, 850);
    startTimerHz (30);
}

MemoryAudioProcessorEditor::~MemoryAudioProcessorEditor()
{
    stopTimer();
    mixSlider.setLookAndFeel (nullptr);
    outputSlider.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
}

void MemoryAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (palette::background);
    graphics.setColour (palette::text);
    graphics.setFont (juce::FontOptions { 28.0f, juce::Font::bold });
    graphics.drawText ("MEMORY", 34, 18, getWidth() - 68, 34,
                       juce::Justification::centredLeft);
    graphics.setColour (palette::memory);
    graphics.fillRect (34, 57, 54, 2);
    graphics.setColour (palette::muted);
    graphics.setFont (juce::FontOptions { 11.0f });
    graphics.drawText ("LIVING SYNTHETIC SOUND ECOLOGY", 100, 45, 250, 18,
                       juce::Justification::centredLeft);
}

void MemoryAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (34);
    area.removeFromTop (48);
    timeline.setBounds (area.removeFromTop (136));
    area.removeFromTop (15);

    auto firstRow = area.removeFromTop (145);
    const auto firstWidth = firstRow.getWidth() / 4;
    for (auto* control : std::initializer_list<ParameterControl*> {
             &memoryLength, &age, &recall, &fragment })
        control->setBounds (firstRow.removeFromLeft (firstWidth).reduced (5, 0));

    auto secondRow = area.removeFromTop (145);
    const auto secondWidth = secondRow.getWidth() / 5;
    for (auto* control : std::initializer_list<ParameterControl*> {
             &decay, &corruption, &drift, &repeat, &feedback })
        control->setBounds (secondRow.removeFromLeft (secondWidth).reduced (3, 0));

    area.removeFromTop (4);
    auto bottom = area.removeFromTop (58);
    auto outputArea = bottom.removeFromRight (190);
    mixLabel.setBounds (bottom.removeFromTop (20));
    mixSlider.setBounds (bottom.removeFromTop (38).reduced (35, 2));
    outputLabel.setBounds (outputArea.removeFromTop (20));
    outputSlider.setBounds (outputArea.removeFromTop (38).reduced (4, 2));

    presetLabel.setBounds (getWidth() - 300, 22, 55, 28);
    presetBox.setBounds (getWidth() - 238, 22, 204, 28);
}

void MemoryAudioProcessorEditor::timerCallback()
{
    timeline.repaint();
    const auto selectedId = processor.getCurrentFactoryPreset() + 1;
    if (presetBox.getSelectedId() != selectedId)
        presetBox.setSelectedId (selectedId, juce::dontSendNotification);
}
