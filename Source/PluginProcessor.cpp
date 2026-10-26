#include "PluginEditor.h"

BoomBapEditor::BoomBapEditor (BoomBapProcessor& p)
    : AudioProcessorEditor (&p), proc (p), programmer (p), fx (p)
{
    setLookAndFeel (&laf);
    laf.setColour (juce::Slider::rotarySliderFillColourId, ui::accent);
    laf.setColour (juce::Slider::rotarySliderOutlineColourId, ui::panel2);
    laf.setColour (juce::Slider::thumbColourId, ui::text);
    laf.setColour (juce::Slider::trackColourId, ui::accent);
    laf.setColour (juce::Slider::textBoxTextColourId, ui::text);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    laf.setColour (juce::ComboBox::textColourId, ui::text);
    laf.setColour (juce::ComboBox::outlineColourId, ui::dim.withAlpha (0.3f));
    laf.setColour (juce::ComboBox::arrowColourId, ui::accent);
    laf.setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    laf.setColour (juce::PopupMenu::textColourId, ui::text);
    laf.setColour (juce::TextButton::buttonColourId, ui::panel2);
    laf.setColour (juce::TextButton::buttonOnColourId, ui::accent);
    laf.setColour (juce::TextButton::textColourOffId, ui::text);
    laf.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    laf.setColour (juce::Label::textColourId, ui::text);

    addAndMakeVisible (programmer);
    addChildComponent (fx);

    for (auto* b : { &tabProgrammer, &tabFx })
    {
        b->setRadioGroupId (1);
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    tabProgrammer.setToggleState (true, juce::dontSendNotification);
    tabProgrammer.onClick = [this] { showTab (0); };
    tabFx.onClick = [this] { showTab (1); };

    rollKitButton.onClick = [this] { proc.rollKit(); };
    addAndMakeVisible (rollKitButton);

    playButton.setClickingTogglesState (true);
    playButton.setToggleState (proc.internalPlay.load(), juce::dontSendNotification);
    playButton.setTooltip ("Internal clock for standalone use. Ignored when your DAW supplies transport.");
    playButton.onClick = [this] { proc.internalPlay.store (playButton.getToggleState()); };
    addAndMakeVisible (playButton);

    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 22);
    tempoSlider.setTextValueSuffix (" BPM");
    addAndMakeVisible (tempoSlider);
    tempoA = std::make_unique<SliderAttach> (proc.apvts, "int_bpm", tempoSlider);

    setSize (1000, 660);
}

BoomBapEditor::~BoomBapEditor()
{
    setLookAndFeel (nullptr);
}

void BoomBapEditor::showTab (int index)
{
    programmer.setVisible (index == 0);
    fx.setVisible (index == 1);
    rollKitButton.setVisible (index == 0);
}

void BoomBapEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::accent);
    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText ("BOOM BAP DRUMS", 16, 8, 230, 28, juce::Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (juce::FontOptions (11.f));
    g.drawText ("MIDI 36-43  |  click steps: normal / accent / ghost", 16, 34, 280, 16, juce::Justification::centredLeft);
}

void BoomBapEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto header = area.removeFromTop (46);

    header.removeFromLeft (300);
    tabProgrammer.setBounds (header.removeFromLeft (120).reduced (2, 6));
    tabFx.setBounds (header.removeFromLeft (100).reduced (2, 6));
    header.removeFromLeft (10);
    rollKitButton.setBounds (header.removeFromLeft (150).reduced (2, 6));

    playButton.setBounds (header.removeFromRight (110).reduced (2, 6));
    tempoSlider.setBounds (header.removeFromRight (170).reduced (2, 8));

    area.removeFromTop (6);
    programmer.setBounds (area);
    fx.setBounds (area);
}
