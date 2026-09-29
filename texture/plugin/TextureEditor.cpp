#include "TextureEditor.h"

#include <algorithm>
#include <cmath>

namespace texturePalette
{
const auto background = juce::Colour::fromRGB (14, 18, 25);
const auto panel = juce::Colour::fromRGB (23, 29, 39);
const auto edge = juce::Colour::fromRGB (48, 59, 75);
const auto text = juce::Colour::fromRGB (230, 235, 240);
const auto muted = juce::Colour::fromRGB (128, 143, 158);
const auto cyan = juce::Colour::fromRGB (74, 198, 222);
const auto violet = juce::Colour::fromRGB (154, 111, 232);
const auto amber = juce::Colour::fromRGB (234, 166, 79);
const auto green = juce::Colour::fromRGB (95, 210, 164);
} 
TextureLookAndFeel::TextureLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, texturePalette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, texturePalette::panel);
    setColour (juce::ComboBox::textColourId, texturePalette::text);
    setColour (juce::ComboBox::outlineColourId, texturePalette::edge);
    setColour (juce::ComboBox::arrowColourId, texturePalette::cyan);
    setColour (juce::PopupMenu::backgroundColourId, texturePalette::panel);
    setColour (juce::PopupMenu::textColourId, texturePalette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, texturePalette::violet.withAlpha (0.4f));
    setColour (juce::TextButton::textColourOffId, texturePalette::muted);
    setColour (juce::TextButton::textColourOnId, texturePalette::text);
}

void TextureLookAndFeel::drawRotarySlider (juce::Graphics& graphics, const int x, const int y,
                                            const int width, const int height,
                                            const float position, const float startAngle,
                                            const float endAngle, juce::Slider& slider)
{
    const auto diameter = static_cast<float> (std::min (width, height)) - 9.0f;
    const auto bounds = juce::Rectangle<float> (
        static_cast<float> (x) + (static_cast<float> (width) - diameter) * 0.5f,
        static_cast<float> (y) + 4.0f, diameter, diameter);
    const auto thickness = std::max (2.0f, diameter * 0.075f);
    const auto angle = startAngle + position * (endAngle - startAngle);
    graphics.setColour (texturePalette::edge);
    graphics.drawEllipse (bounds.reduced (thickness * 0.5f), thickness);
    juce::Path arc;
    arc.addCentredArc (bounds.getCentreX(), bounds.getCentreY(),
                       bounds.getWidth() * 0.5f - thickness,
                       bounds.getHeight() * 0.5f - thickness,
                       0.0f, startAngle, angle, true);
    graphics.setColour (texturePalette::cyan);
    graphics.strokePath (arc, juce::PathStrokeType (thickness,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto centre = bounds.getCentre();
    const auto tip = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                  * diameter * 0.26f;
    graphics.setColour (texturePalette::text);
    graphics.drawLine ({ centre, tip }, 1.8f);
    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (texturePalette::amber);
        graphics.drawRoundedRectangle (bounds.expanded (4.0f), 6.0f, 1.5f);
    }
}

void TextureLookAndFeel::drawLinearSlider (juce::Graphics& graphics, const int x, const int y,
                                            const int width, const int height,
                                            const float sliderPosition, float, float,
                                            const juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
        return LookAndFeel_V4::drawLinearSlider (graphics, x, y, width, height,
                                                  sliderPosition, 0.0f, 0.0f, style, slider);
    const auto centreY = static_cast<float> (y + height / 2);
    graphics.setColour (texturePalette::edge);
    graphics.drawLine (static_cast<float> (x), centreY,
                       static_cast<float> (x + width), centreY, 4.0f);
    graphics.setColour (texturePalette::cyan);
    graphics.drawLine (static_cast<float> (x), centreY, sliderPosition, centreY, 4.0f);
    graphics.fillEllipse (sliderPosition - 6.0f, centreY - 6.0f, 12.0f, 12.0f);
    if (slider.hasKeyboardFocus (true))
    {
        graphics.setColour (texturePalette::amber);
        graphics.drawRoundedRectangle (juce::Rectangle<float> (
            static_cast<float> (x), static_cast<float> (y),
            static_cast<float> (width), static_cast<float> (height)), 4.0f, 1.5f);
    }
}

void TextureLookAndFeel::drawButtonBackground (juce::Graphics& graphics, juce::Button& button,
                                                const juce::Colour&, const bool highlighted,
                                                const bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto colour = button.getToggleState() ? texturePalette::violet : texturePalette::panel;
    if (highlighted)
        colour = colour.brighter (0.12f);
    if (down)
        colour = colour.darker (0.15f);
    graphics.setColour (colour);
    graphics.fillRoundedRectangle (bounds, 6.0f);
    graphics.setColour (button.hasKeyboardFocus (true) ? texturePalette::amber : texturePalette::edge);
    graphics.drawRoundedRectangle (bounds, 6.0f, button.hasKeyboardFocus (true) ? 2.0f : 1.0f);
}

TextureParameterControl::TextureParameterControl (
    juce::AudioProcessorValueTreeState& state, const juce::String& id,
    const juce::String& title, const juce::String& tooltip,
    TextureLookAndFeel& lookAndFeel, const bool horizontal)
    : isHorizontal (horizontal)
{
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, texturePalette::muted);
    label.setFont (juce::FontOptions { 10.5f, juce::Font::bold });
    addAndMakeVisible (label);
    slider.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal
                                      : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (horizontal ? juce::Slider::TextBoxRight
                                       : juce::Slider::TextBoxBelow,
                            false, horizontal ? 66 : 72, 19);
    slider.setLookAndFeel (&lookAndFeel);
    slider.setTitle (title);
    slider.setDescription (tooltip);
    slider.setTooltip (tooltip);
    slider.setWantsKeyboardFocus (true);
    if (const auto* parameter = state.getParameter (id))
        slider.setDoubleClickReturnValue (true,
            parameter->convertFrom0to1 (parameter->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, id, slider);
}

TextureParameterControl::~TextureParameterControl()
{
    slider.setLookAndFeel (nullptr);
}

void TextureParameterControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (18));
    slider.setBounds (isHorizontal ? area.reduced (4, 4) : area);
}

void TextureSurfaceDisplay::paint (juce::Graphics& graphics)
{
    const auto snapshot = processor.getVisualizationSnapshot();
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour (texturePalette::panel);
    graphics.fillRoundedRectangle (bounds, 11.0f);
    graphics.setColour (snapshot.frozen ? texturePalette::violet : texturePalette::edge);
    graphics.drawRoundedRectangle (bounds.reduced (0.5f), 11.0f, snapshot.frozen ? 1.8f : 1.0f);
    auto plot = bounds.reduced (26.0f, 25.0f);
    graphics.setColour (texturePalette::edge.withAlpha (0.45f));
    for (int line = 1; line < 6; ++line)
        graphics.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * line / 6.0f),
                                   plot.getY(), plot.getBottom());
    const auto grains = std::min<std::size_t> (snapshot.activeGrains, 32);
    for (std::size_t index = 0; index < 32; ++index)
    {
        const auto active = index < grains;
        const auto normalized = static_cast<float> (index) / 31.0f;
        const auto x = plot.getX() + normalized * plot.getWidth();
        const auto wave = std::sin (phase * (0.65f + normalized * 0.8f) + normalized * 17.0f);
        const auto y = plot.getCentreY() + wave * plot.getHeight()
                                             * (0.12f + snapshot.surfaceEnergy * 0.31f);
        const auto radius = active ? 2.2f + snapshot.surfaceEnergy * 3.6f : 1.2f;
        graphics.setColour ((active ? texturePalette::cyan : texturePalette::edge)
                                .withAlpha (active ? 0.9f : 0.35f));
        graphics.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
    graphics.setColour (snapshot.frozen ? texturePalette::violet : texturePalette::muted);
    graphics.setFont (juce::FontOptions { 10.5f, juce::Font::bold });
    graphics.drawText (snapshot.frozen ? "SPECTRUM FROZEN" : "LIVE MICROSTRUCTURE",
                       16, 7, getWidth() - 32, 18, juce::Justification::centred);
    const auto meterHeight = plot.getHeight();
    const auto inputHeight = meterHeight * std::sqrt (snapshot.inputLevel);
    const auto outputHeight = meterHeight * std::sqrt (snapshot.outputLevel);
    graphics.setColour (texturePalette::green);
    graphics.fillRoundedRectangle (8.0f, plot.getBottom() - inputHeight, 4.0f, inputHeight, 2.0f);
    graphics.setColour (texturePalette::amber);
    graphics.fillRoundedRectangle (bounds.getRight() - 12.0f, plot.getBottom() - outputHeight,
                                   4.0f, outputHeight, 2.0f);
}

TextureAudioProcessorEditor::TextureAudioProcessorEditor (TextureAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner), surface (owner),
      capture (owner.getParameterState(), "capture", "CAPTURE", "Amount of input feeding the texture cloud", lookAndFeel),
      grainSize (owner.getParameterState(), "grain_size", "GRAIN SIZE", "Duration of each captured fragment", lookAndFeel),
      density (owner.getParameterState(), "density", "DENSITY", "Number of grains created per second", lookAndFeel),
      spray (owner.getParameterState(), "spray", "SPRAY", "Distance into recent audio used for grain starts", lookAndFeel),
      pitch (owner.getParameterState(), "pitch", "PITCH", "Pitch centre in semitones", lookAndFeel),
      reverse (owner.getParameterState(), "reverse", "REVERSE", "Probability that a grain plays backwards", lookAndFeel),
      blur (owner.getParameterState(), "blur", "BLUR", "Spectral and temporal smoothing", lookAndFeel),
      detail (owner.getParameterState(), "detail", "DETAIL", "Input-following micro-detail and excitation", lookAndFeel),
      wear (owner.getParameterState(), "wear", "WEAR", "Saturation, bit erosion and rate erosion", lookAndFeel),
      dropout (owner.getParameterState(), "dropout", "DROPOUT", "Depth and frequency of click-safe signal losses", lookAndFeel),
      tone (owner.getParameterState(), "tone", "TONE", "Dark-to-bright spectral tilt", lookAndFeel),
      motionRate (owner.getParameterState(), "motion_rate", "RATE", "Speed of deterministic texture motion", lookAndFeel),
      motionDepth (owner.getParameterState(), "motion_depth", "DEPTH", "Amount of position, pitch and stereo motion", lookAndFeel),
      width (owner.getParameterState(), "width", "WIDTH", "Stereo spread with mono-safe centre", lookAndFeel),
      seed (owner.getParameterState(), "seed", "SEED", "Repeatable random sequence", lookAndFeel),
      input (owner.getParameterState(), "input", "INPUT", "Input trim", lookAndFeel, true),
      mix (owner.getParameterState(), "mix", "DRY / WET", "Equal-power dry and texture blend", lookAndFeel, true),
      output (owner.getParameterState(), "output", "OUTPUT", "Final output trim", lookAndFeel, true)
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &surface, &capture, &grainSize, &density, &spray, &pitch, &reverse, &blur,
             &detail, &wear, &dropout, &tone, &motionRate, &motionDepth, &width, &seed,
             &input, &mix, &output, &freezeButton, &reduceMotionButton, &presetLabel, &presetBox })
        addAndMakeVisible (component);
    freezeButton.setLookAndFeel (&lookAndFeel);
    freezeButton.setClickingTogglesState (true);
    freezeButton.setTooltip ("Latch the current spectral surface; click again to release");
    freezeButton.setTitle ("FREEZE");
    freezeButton.setWantsKeyboardFocus (true);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        owner.getParameterState(), "freeze", freezeButton);
    reduceMotionButton.setLookAndFeel (&lookAndFeel);
    reduceMotionButton.setClickingTogglesState (true);
    reduceMotionButton.setTooltip ("Stop decorative animation without changing the sound");
    reduceMotionButton.setWantsKeyboardFocus (true);
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, texturePalette::muted);
    presetLabel.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetBox.setLookAndFeel (&lookAndFeel);
    presetBox.setTooltip ("Factory texture starting points");
    presetBox.setWantsKeyboardFocus (true);
    for (int index = 0; index < owner.getNumFactoryPresets(); ++index)
        presetBox.addItem (owner.getFactoryPresetName (index), index + 1);
    presetBox.setSelectedId (owner.getCurrentFactoryPreset() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.applyFactoryPreset (presetBox.getSelectedId() - 1); };
    setSize (980, 720);
    setResizable (true, true);
    setResizeLimits (820, 640, 1380, 980);
    startTimerHz (30);
}

TextureAudioProcessorEditor::~TextureAudioProcessorEditor()
{
    stopTimer();
    freezeButton.setLookAndFeel (nullptr);
    reduceMotionButton.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
}

void TextureAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (texturePalette::background);
    graphics.setColour (texturePalette::text);
    graphics.setFont (juce::FontOptions { 27.0f, juce::Font::bold });
    graphics.drawText ("TEXTURE", 28, 14, 190, 34, juce::Justification::centredLeft);
    graphics.setColour (texturePalette::cyan);
    graphics.fillRect (28, 53, 58, 2);
    graphics.setColour (texturePalette::muted);
    graphics.setFont (juce::FontOptions { 10.5f });
    graphics.drawText ("LIVING SYNTHETIC SOUND ECOLOGY", 98, 41, 250, 18, juce::Justification::centredLeft);
    constexpr std::array<const char*, 4> titles { "CAPTURE", "STRUCTURE", "AGE", "MOTION" };
    const std::array<juce::Colour, 4> colours {
        texturePalette::cyan, texturePalette::violet,
        texturePalette::amber, texturePalette::green
    };
    for (std::size_t index = 0; index < groupBounds.size(); ++index)
    {
        auto bounds = groupBounds[index].toFloat();
        graphics.setColour (texturePalette::panel);
        graphics.fillRoundedRectangle (bounds, 9.0f);
        graphics.setColour (texturePalette::edge);
        graphics.drawRoundedRectangle (bounds.reduced (0.5f), 9.0f, 1.0f);
        graphics.setColour (colours[index]);
        graphics.setFont (juce::FontOptions { 10.5f, juce::Font::bold });
        graphics.drawText (titles[index], groupBounds[index].reduced (12, 5).removeFromTop (16),
                           juce::Justification::centredLeft);
    }
}

void TextureAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (45);
    auto headerRight = getLocalBounds().reduced (28).removeFromTop (32);
    reduceMotionButton.setBounds (headerRight.removeFromRight (145));
    headerRight.removeFromRight (8);
    presetBox.setBounds (headerRight.removeFromRight (210));
    presetLabel.setBounds (headerRight.removeFromRight (58));
    surface.setBounds (area.removeFromTop (148));
    area.removeFromTop (12);
    auto groups = area.removeFromTop (area.getHeight() - 72);
    const auto groupWidth = groups.getWidth() / 4;
    for (std::size_t index = 0; index < groupBounds.size(); ++index)
        groupBounds[index] = groups.removeFromLeft (index == groupBounds.size() - 1
                                                        ? groups.getWidth() : groupWidth)
                                  .reduced (index == 0 ? 0 : 4, 0);
    layoutFour (groupBounds[0], { &capture, &grainSize, &density, &spray });
    layoutFour (groupBounds[1], { &pitch, &reverse, &blur, &freezeButton });
    layoutFour (groupBounds[2], { &detail, &wear, &dropout, &tone });
    layoutFour (groupBounds[3], { &motionRate, &motionDepth, &width, &seed });
    area.removeFromTop (10);
    auto bottom = area.removeFromTop (62);
    const auto faderWidth = bottom.getWidth() / 3;
    input.setBounds (bottom.removeFromLeft (faderWidth).reduced (5, 0));
    mix.setBounds (bottom.removeFromLeft (faderWidth).reduced (5, 0));
    output.setBounds (bottom.reduced (5, 0));
}

void TextureAudioProcessorEditor::layoutFour (
    juce::Rectangle<int> bounds, const std::array<juce::Component*, 4>& controls)
{
    auto content = bounds.reduced (8);
    content.removeFromTop (20);
    const auto rowHeight = content.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowBounds = content.removeFromTop (row == 0 ? rowHeight : content.getHeight());
        const auto widthPerControl = rowBounds.getWidth() / 2;
        controls[static_cast<std::size_t> (row * 2)]->setBounds (
            rowBounds.removeFromLeft (widthPerControl).reduced (2));
        controls[static_cast<std::size_t> (row * 2 + 1)]->setBounds (rowBounds.reduced (2));
    }
}

void TextureAudioProcessorEditor::timerCallback()
{
    if (! reduceMotionButton.getToggleState())
        visualPhase += 0.065f;
    surface.setVisualPhase (visualPhase);
    surface.repaint();
    const auto selected = processor.getCurrentFactoryPreset() + 1;
    if (presetBox.getSelectedId() != selected)
        presetBox.setSelectedId (selected, juce::dontSendNotification);
}
