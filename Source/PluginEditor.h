#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg (0xff141519), panel (0xff1d1f26), panel2 (0xff2a2d38),
                   accent (0xffff8a2a), accent2 (0xff39c5bb), text (0xffe9e9ee), dim (0xff8b8e9a);
}

using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

//==============================================================================
class StepButton : public juce::Component
{
public:
    std::function<void()> onClick;
    int velocity = 0, index = 0;
    bool isPlayhead = false;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        g.setColour ((index / 4) % 2 == 0 ? ui::panel2 : ui::panel2.brighter (0.12f));
        g.fillRoundedRectangle (b, 4.f);
        if (velocity > 0)
        {
            g.setColour (ui::accent.withAlpha (0.35f + 0.65f * (float) velocity / 127.f));
            g.fillRoundedRectangle (b, 4.f);
        }
        if (isPlayhead)
        {
            g.setColour (ui::accent2);
            g.drawRoundedRectangle (b, 4.f, 2.f);
        }
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

//==============================================================================
// Left-click = play the pad, right-click = settings, drop a WAV on it = replace sample.
class PadButton : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    std::function<void()> onLeftClick, onRightClick;
    std::function<void (const juce::File&)> onFileDropped;
    juce::String title, subtitle;
    float flash = 0.f;
    bool dragOver = false;

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg")) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        for (auto& f : files)
        {
            const juce::File file (f);
            if (file.hasFileExtension ("wav;aif;aiff;flac;ogg")) { if (onFileDropped) onFileDropped (file); break; }
        }
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); }
        else { flash = 1.f; if (onLeftClick) onLeftClick(); repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        g.setColour (ui::panel2.interpolatedWith (ui::accent, flash * 0.8f));
        g.fillRoundedRectangle (b, 8.f);
        g.setColour (dragOver ? ui::accent2 : ui::dim.withAlpha (0.4f));
        g.drawRoundedRectangle (b, 8.f, dragOver ? 3.f : 1.f);

        auto t = getLocalBounds().reduced (10, 6);
        g.setColour (ui::text);
        g.setFont (juce::FontOptions (15.f, juce::Font::bold));
        g.drawText (title, t.removeFromTop (t.getHeight() / 2), juce::Justification::centredLeft);
        g.setColour (ui::dim);
        g.setFont (juce::FontOptions (12.f));
        g.drawFittedText (dragOver ? "Drop to load" : subtitle, t, juce::Justification::centredLeft, 1);
    }
};

//==============================================================================
// Right-click popup for a pad: volume knob, category, snap, roll, clear, load.
class PadSettings : public juce::Component
{
public:
    PadSettings (BoomBapProcessor& p, int padIndex) : proc (p), pad (padIndex)
    {
        const juce::String n (pad + 1);

        title.setText ("PAD " + n, juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, ui::accent);
        title.setFont (juce::FontOptions (16.f, juce::Font::bold));
        addAndMakeVisible (title);

        vol.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        vol.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        vol.setTextValueSuffix (" dB");
        vol.setDoubleClickReturnValue (true, -3.0);
        addAndMakeVisible (vol);

        cat.addItemList (bb::categoryNames(), 1);
        snap.addItemList (bb::snapNames(), 1);
        addAndMakeVisible (cat);
        addAndMakeVisible (snap);

        volA  = std::make_unique<SliderAttach> (proc.apvts, "pad" + n + "_vol", vol);
        catA  = std::make_unique<ComboAttach>  (proc.apvts, "pad" + n + "_cat", cat);
        snapA = std::make_unique<ComboAttach>  (proc.apvts, "pad" + n + "_snap", snap);

        for (auto* b : { &roll, &clear, &load }) addAndMakeVisible (b);
        roll.onClick  = [this] { proc.rollPad (pad); };
        clear.onClick = [this] { proc.clearPad (pad); };
        load.onClick  = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [&p = proc, i = pad] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile() && p.loadSampleFile (i, f)) p.triggerPad (i);
                                  });
        };
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (12);
        title.setBounds (b.removeFromTop (22));
        vol.setBounds (b.removeFromTop (130));
        b.removeFromTop (6);
        cat.setBounds (b.removeFromTop (26)); b.removeFromTop (6);
        snap.setBounds (b.removeFromTop (26)); b.removeFromTop (8);
        auto row = b.removeFromTop (26);
        roll.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (0, 0).withTrimmedRight (3));
        clear.setBounds (row.withTrimmedLeft (3));
        b.removeFromTop (6);
        load.setBounds (b.removeFromTop (26));
    }

private:
    BoomBapProcessor& proc;
    int pad;
    juce::Label title;
    juce::Slider vol;
    juce::ComboBox cat, snap;
    juce::TextButton roll { "Roll groove" }, clear { "Clear" }, load { "Load sample..." };
    std::unique_ptr<SliderAttach> volA;
    std::unique_ptr<ComboAttach> catA, snapA;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class PadRow : public juce::Component
{
public:
    PadRow (BoomBapProcessor& p, int index) : proc (p), pad (index)
    {
        const juce::String n (pad + 1);
        padButton.title = "PAD " + n + "  " + juce::MidiMessage::getMidiNoteName (bb::kFirstNote + pad, true, true, 3);
        padButton.onLeftClick  = [this] { proc.triggerPad (pad); };
        padButton.onFileDropped = [this] (const juce::File& f)
        {
            if (proc.loadSampleFile (pad, f)) proc.triggerPad (pad);
        };
        padButton.onRightClick = [this]
        {
            auto content = std::make_unique<PadSettings> (proc, pad);
            content->setSize (250, 290);
            juce::CallOutBox::launchAsynchronously (std::move (content), padButton.getScreenBounds(), nullptr);
        };
        addAndMakeVisible (padButton);

        catBox.addItemList (bb::categoryNames(), 1);
        snapBox.addItemList (bb::snapNames(), 1);
        addAndMakeVisible (catBox);
        addAndMakeVisible (snapBox);
        catA  = std::make_unique<ComboAttach> (proc.apvts, "pad" + n + "_cat", catBox);
        snapA = std::make_unique<ComboAttach> (proc.apvts, "pad" + n + "_snap", snapBox);

        rollButton.setTooltip ("Roll a new groove + swing for this sound only");
        rollButton.onClick = [this] { proc.rollPad (pad); };
        addAndMakeVisible (rollButton);

        for (int s = 0; s < bb::kSteps; ++s)
        {
            steps[s].index = s;
            steps[s].onClick = [this, s]
            {
                // off -> normal -> accent -> ghost -> off
                const int v = proc.getStep (pad, s);
                proc.setStep (pad, s, v == 0 ? 100 : v <= 55 ? 0 : v <= 105 ? 127 : 45);
            };
            addAndMakeVisible (steps[s]);
        }
    }

    void refresh (int playheadStep)
    {
        for (int s = 0; s < bb::kSteps; ++s)
        {
            const int v = proc.getStep (pad, s);
            const bool ph = (s == playheadStep);
            if (steps[s].velocity != v || steps[s].isPlayhead != ph)
            {
                steps[s].velocity = v;
                steps[s].isPlayhead = ph;
                steps[s].repaint();
            }
        }
        const auto name = proc.getSampleName (pad);
        if (padButton.subtitle != name || padButton.flash > 0.f)
        {
            padButton.subtitle = name;
            padButton.flash = juce::jmax (0.f, padButton.flash - 0.12f);
            padButton.repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (ui::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 8.f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 4);
        padButton.setBounds (b.removeFromLeft (150));
        b.removeFromLeft (6);
        auto col = b.removeFromLeft (118);
        catBox.setBounds (col.removeFromTop (col.getHeight() / 2).reduced (0, 2));
        snapBox.setBounds (col.reduced (0, 2));
        b.removeFromLeft (6);
        rollButton.setBounds (b.removeFromLeft (46).reduced (0, 12));
        b.removeFromLeft (8);
        const int w = b.getWidth() / bb::kSteps;
        for (int s = 0; s < bb::kSteps; ++s) steps[s].setBounds (b.removeFromLeft (w));
    }

private:
    BoomBapProcessor& proc;
    int pad;
    PadButton padButton;
    juce::ComboBox catBox, snapBox;
    juce::TextButton rollButton { "Roll" };
    StepButton steps[bb::kSteps];
    std::unique_ptr<ComboAttach> catA, snapA;
};

//==============================================================================
class ProgrammerPanel : public juce::Component, private juce::Timer
{
public:
    explicit ProgrammerPanel (BoomBapProcessor& p) : proc (p)
    {
        for (int i = 0; i < bb::kPads; ++i)
        {
            rows[i] = std::make_unique<PadRow> (proc, i);
            addAndMakeVisible (*rows[i]);
        }
        startTimerHz (30);
    }
    ~ProgrammerPanel() override { stopTimer(); }

    void resized() override
    {
        auto b = getLocalBounds();
        const int h = b.getHeight() / bb::kPads;
        for (auto& r : rows) r->setBounds (b.removeFromTop (h));
    }

private:
    void timerCallback() override
    {
        const int step = proc.currentStep.load();
        for (auto& r : rows) r->refresh (step);
    }
    BoomBapProcessor& proc;
    std::unique_ptr<PadRow> rows[bb::kPads];
};

//==============================================================================
class FxPanel : public juce::Component
{
public:
    explicit FxPanel (BoomBapProcessor& p) : proc (p)
    {
        setup (bits, "crush_bits", "Bit Depth", " bit");
        setup (rate, "crush_rate", "Sample Rate", " Hz");
        setup (drive, "sat_drive", "Drive");
        setup (mix, "sat_mix", "Mix");
        setup (low, "lowcut", "Low Cut", " Hz");
        setup (high, "highcut", "High Cut", " Hz");
        setup (eqF, "eq_freq", "Frequency", " Hz");
        setup (eqG, "eq_gain", "Gain", " dB");
        setup (eqQ, "eq_q", "Q");

        satType.addItemList (juce::StringArray { "Off", "Warm Tube", "Cassette", "Vinyl" }, 1);
        addAndMakeVisible (satType);
        satTypeA = std::make_unique<ComboAttach> (proc.apvts, "sat_type", satType);

        const char* names[] = { "SP-1200", "Dusty Vinyl", "Tape Deck", "Reset FX" };
        for (int i = 0; i < 4; ++i)
        {
            presets[i].setButtonText (names[i]);
            presets[i].onClick = [this, i] { proc.applyFxPreset (i); };
            addAndMakeVisible (presets[i]);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const std::pair<const char*, juce::Rectangle<int>> secs[] = {
            { "SAMPLER GRIT", gritR }, { "ANALOG WARMTH", warmR }, { "FILTERS", filtR }, { "PARAMETRIC EQ", eqR } };
        for (auto& [name, r] : secs)
        {
            g.setColour (ui::panel);
            g.fillRoundedRectangle (r.toFloat(), 10.f);
            g.setColour (ui::accent);
            g.setFont (juce::FontOptions (14.f, juce::Font::bold));
            g.drawText (name, r.reduced (14, 8).removeFromTop (20), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);
        auto bottom = area.removeFromBottom (46);
        auto top = area.removeFromTop (area.getHeight() / 2);
        gritR = top.removeFromLeft (top.getWidth() / 2).reduced (6);
        warmR = top.reduced (6);
        filtR = area.removeFromLeft (area.getWidth() / 2).reduced (6);
        eqR = area.reduced (6);

        layoutKnobs (gritR, { &bits, &rate });
        auto w = warmR;
        satType.setBounds (w.withTrimmedTop (34).removeFromTop (30).reduced (40, 0));
        layoutKnobs (w.withTrimmedTop (34), { &drive, &mix });
        layoutKnobs (filtR, { &low, &high });
        layoutKnobs (eqR, { &eqF, &eqG, &eqQ });

        const int bw = 130;
        auto row = bottom.withSizeKeepingCentre (bw * 4 + 18, 32);
        for (auto& b : presets) { b.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (6); }
    }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttach> att;
    };

    void setup (Knob& k, const juce::String& id, const juce::String& name, const juce::String& suffix = {})
    {
        k.slider.setTextValueSuffix (suffix);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.label.setText (name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, ui::dim);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.att = std::make_unique<SliderAttach> (proc.apvts, id, k.slider);
    }

    void layoutKnobs (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        r.removeFromTop (30);
        const int w = r.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = r.removeFromLeft (w).reduced (6, 0);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell);
        }
    }

    BoomBapProcessor& proc;
    Knob bits, rate, drive, mix, low, high, eqF, eqG, eqQ;
    juce::ComboBox satType;
    std::unique_ptr<ComboAttach> satTypeA;
    juce::TextButton presets[4];
    juce::Rectangle<int> gritR, warmR, filtR, eqR;
};

//==============================================================================
class BoomBapEditor : public juce::AudioProcessorEditor
{
public:
    explicit BoomBapEditor (BoomBapProcessor&);
    ~BoomBapEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showTab (int index);

    BoomBapProcessor& proc;
    juce::LookAndFeel_V4 laf;
    juce::TextButton tabProgrammer { "PROGRAMMER" }, tabFx { "FX RACK" },
                     rollKitButton { "Roll 8-Pad Groove" }, playButton { "Internal Play" };
    juce::Slider tempoSlider;
    std::unique_ptr<SliderAttach> tempoA;
    ProgrammerPanel programmer;
    FxPanel fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomBapEditor)
};
