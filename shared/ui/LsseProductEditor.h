#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace lsse::ui
{
struct SectionSpec
{
    juce::String name;
    std::vector<std::string> parameterIds;
};

struct PresetSpec
{
    juce::String name;
    std::vector<std::pair<std::string, float>> normalisedValues;
};

struct ActionSpec
{
    juce::String name;
    std::function<void()> callback;
};

class ParameterControl final : public juce::Component
{
public:
    ParameterControl (juce::AudioProcessorValueTreeState& state, const std::string& id, juce::Colour accent)
    {
        auto* parameter = state.getParameter (id);
        jassert (parameter != nullptr);
        title.setText (parameter != nullptr ? parameter->getName (64) : id, juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, juce::Colour (0xffaab5c2));
        title.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        addAndMakeVisible (title);

        setTitle (title.getText());
        setDescription ("LSSE parameter " + title.getText());
        setHelpText ("Adjust " + title.getText());

        if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            toggle = std::make_unique<juce::ToggleButton> (title.getText());
            toggle->setButtonText ("ON");
            toggle->setColour (juce::ToggleButton::tickColourId, accent);
            toggle->setColour (juce::ToggleButton::textColourId, juce::Colours::white);
            toggle->setTitle (title.getText());
            addAndMakeVisible (*toggle);
            buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *toggle);
        }
        else if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            combo = std::make_unique<juce::ComboBox>();
            int item = 1;
            for (const auto& value : parameter->getAllValueStrings()) combo->addItem (value, item++);
            combo->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202832));
            combo->setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.55f));
            combo->setColour (juce::ComboBox::textColourId, juce::Colours::white);
            combo->setTitle (title.getText());
            addAndMakeVisible (*combo);
            comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *combo);
        }
        else
        {
            slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                     juce::Slider::TextBoxBelow);
            slider->setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff35414e));
            slider->setColour (juce::Slider::thumbColourId, juce::Colours::white);
            slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider->setTitle (title.getText());
            addAndMakeVisible (*slider);
            sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *slider);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds (area.removeFromTop (22));
        if (slider) slider->setBounds (area.reduced (2));
        if (combo) combo->setBounds (area.withSizeKeepingCentre (juce::jmax (84, area.getWidth() - 8), 28));
        if (toggle) toggle->setBounds (area.withSizeKeepingCentre (58, 26));
    }

private:
    juce::Label title;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
};

class SectionView final : public juce::Component
{
public:
    SectionView (juce::AudioProcessorValueTreeState& state, const SectionSpec& spec, juce::Colour accent)
        : accentColour (accent)
    {
        heading.setText (spec.name.toUpperCase(), juce::dontSendNotification);
        heading.setColour (juce::Label::textColourId, accent);
        heading.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        addAndMakeVisible (heading);
        for (const auto& id : spec.parameterIds)
        {
            auto control = std::make_unique<ParameterControl> (state, id, accent);
            addAndMakeVisible (*control);
            controls.push_back (std::move (control));
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff171e26));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (accentColour.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 7);
        heading.setBounds (area.removeFromTop (20));
        if (controls.empty()) return;
        const auto width = area.getWidth() / static_cast<int> (controls.size());
        for (std::size_t i = 0; i < controls.size(); ++i)
        {
            auto cell = i + 1 == controls.size() ? area : area.removeFromLeft (width);
            controls[i]->setBounds (cell.reduced (3, 0));
        }
    }

private:
    juce::Colour accentColour;
    juce::Label heading;
    std::vector<std::unique_ptr<ParameterControl>> controls;
};

class ProductEditor final : public juce::AudioProcessorEditor
{
public:
    ProductEditor (juce::AudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& stateToUse,
                   juce::String productName,
                   juce::String productTagline,
                   juce::Colour accent,
                   std::vector<SectionSpec> sectionSpecs,
                   std::vector<PresetSpec> presetSpecs,
                   std::vector<ActionSpec> actionSpecs = {},
                   std::unique_ptr<juce::Component> featureComponent = {})
        : juce::AudioProcessorEditor (processor), state (stateToUse), name (std::move (productName)),
          tagline (std::move (productTagline)), accentColour (accent),
          feature (std::move (featureComponent))
    {
        setTitle (name + " — LSSE");
        setDescription (tagline);
        for (const auto& spec : sectionSpecs)
        {
            auto section = std::make_unique<SectionView> (state, spec, accentColour);
            addAndMakeVisible (*section);
            sections.push_back (std::move (section));
        }
        for (const auto& preset : presetSpecs)
        {
            auto button = std::make_unique<juce::TextButton> (preset.name);
            button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202a35));
            button->setColour (juce::TextButton::buttonOnColourId, accentColour);
            button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            button->setTitle (preset.name + " preset");
            const auto values = preset.normalisedValues;
            button->onClick = [this, values]
            {
                for (const auto& [id, value] : values)
                    if (auto* parameter = state.getParameter (id))
                    {
                        parameter->beginChangeGesture();
                        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
                        parameter->endChangeGesture();
                    }
            };
            addAndMakeVisible (*button);
            presetButtons.push_back (std::move (button));
        }
        for (auto& action : actionSpecs)
        {
            auto button = std::make_unique<juce::TextButton> (action.name);
            button->setColour (juce::TextButton::buttonColourId, accentColour.withAlpha (0.35f));
            button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            button->setTitle (action.name);
            button->setDescription ("LSSE source or file action");
            button->onClick = std::move (action.callback);
            addAndMakeVisible (*button);
            actionButtons.push_back (std::move (button));
        }
        if (feature != nullptr)
            addAndMakeVisible (*feature);
        setResizable (true, true);
        setResizeLimits (720, 520, 1500, 1000);
        setSize (980, 720);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0e141b));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
        g.drawText (name, 24, 14, getWidth() - 48, 38, juce::Justification::centredLeft);
        g.setColour (accentColour);
        g.fillRect (24, 57, 64, 3);
        g.setColour (juce::Colour (0xff8d9aa8));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (tagline.toUpperCase(), 102, 45, getWidth() - 126, 22, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff647281));
        g.drawText ("LSSE — LIVING SYNTHETIC SOUND ECOLOGY", 24, getHeight() - 24, getWidth() - 48, 16,
                    juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (18);
        area.removeFromTop (62);
        auto presets = area.removeFromTop (38);
        for (auto& button : presetButtons) button->setBounds (presets.removeFromLeft (118).reduced (4, 3));
        for (auto iterator = actionButtons.rbegin(); iterator != actionButtons.rend(); ++iterator)
            (*iterator)->setBounds (presets.removeFromRight (142).reduced (4, 3));
        area.removeFromTop (5);
        area.removeFromBottom (20);
        if (feature != nullptr)
        {
            feature->setBounds (area.removeFromTop (170).reduced (4, 4));
            area.removeFromTop (4);
        }
        if (sections.empty()) return;
        const auto height = area.getHeight() / static_cast<int> (sections.size());
        for (std::size_t i = 0; i < sections.size(); ++i)
        {
            auto row = i + 1 == sections.size() ? area : area.removeFromTop (height);
            sections[i]->setBounds (row.reduced (4, 4));
        }
    }

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String name, tagline;
    juce::Colour accentColour;
    std::vector<std::unique_ptr<SectionView>> sections;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    std::vector<std::unique_ptr<juce::TextButton>> actionButtons;
    std::unique_ptr<juce::Component> feature;
};
}
