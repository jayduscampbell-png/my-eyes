#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Groove.h"

struct Sample
{
    juce::AudioBuffer<float> data;
    double rate = 44100.0;
    juce::String name;
};

class BoomBapProcessor : public juce::AudioProcessor
{
public:
    BoomBapProcessor();
    ~BoomBapProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Boom Bap Drums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- UI-facing API (message thread) ----
    bool loadSampleFile (int pad, const juce::File& file);
    juce::String getSampleName (int pad) const { return sampleName[pad]; }
    void triggerPad (int pad) { uiTrigger[pad].store (true); }
    void rollKit();
    void rollPad (int pad);
    void clearPad (int pad);
    void applyFxPreset (int preset);
    int  getStep (int pad, int step) const { return pattern[pad][step].load(); }
    void setStep (int pad, int step, int velocity) { pattern[pad][step].store (velocity); }

    std::atomic<int> currentStep { -1 };
    std::atomic<bool> internalPlay { false };

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Voice
    {
        std::shared_ptr<const Sample> sample;
        double pos = 0.0, ratio = 1.0;
        float gain = 0.f;
        bool active = false;
    };
    struct Event { int sample; int pad; float vel; };

    bb::Kit readKit() const;
    void writeKit (const bb::Kit& kit, int onlyPad);
    void setChoiceParam (const juce::String& id, int index);
    void setFloatParam (const juce::String& id, float plainValue);

    void addEvent (int sample, int pad, float vel);
    void fire (const Event& e);
    void scheduleSequencer (int numSamples);
    void applyFx (juce::AudioBuffer<float>&);
    void updateFilters();

    juce::AudioFormatManager formatManager;
    juce::Random rng { (juce::int64) juce::Time::currentTimeMillis() };
    juce::SpinLock sampleLock;
    std::shared_ptr<const Sample> samples[bb::kPads];
    juce::String sampleName[bb::kPads], samplePath[bb::kPads];
    std::atomic<int> pattern[bb::kPads][bb::kSteps];
    std::atomic<bool> uiTrigger[bb::kPads];

    Voice voices[bb::kPads];
    Event events[1024];
    int nEvents = 0;
    double sr = 44100.0, internalPpq = 0.0;

    std::atomic<float>* volParam[bb::kPads] {};
    std::atomic<float>* catParam[bb::kPads] {};
    std::atomic<float>* snapParam[bb::kPads] {};
    std::atomic<float> *fxBits {}, *fxRate {}, *fxSatType {}, *fxDrive {}, *fxMix {},
                       *fxLow {}, *fxHigh {}, *fxEqFreq {}, *fxEqGain {}, *fxEqQ {}, *intBpm {};

    // FX state
    float phase[2] {}, held[2] {};
    juce::dsp::IIR::Filter<float> hp[2], lp[2], peak[2], satLP[2];
    float lastLow = -1.f, lastHigh = -1.f, lastEqF = -1.f, lastEqG = -99.f, lastEqQ = -1.f;
    int lastSat = -1;
    juce::Random fxRand;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomBapProcessor)
};
