#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace bb;

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    static const int defCat[kPads]  = { Kick, Snare, HiHat, OpenHat, Clap, Perc, Crash, Perc };
    static const int defSnap[kPads] = { Straight, Straight, MPC58, MPC58, Straight, MPC58, Straight, MPC58 };

    for (int i = 0; i < kPads; ++i)
    {
        const String n (i + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pad" + n + "_vol", 1 }, "Pad " + n + " Volume",
                                                           NormalisableRange<float> (-60.f, 6.f, 0.1f), -3.f,
                                                           AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pad" + n + "_cat", 1 }, "Pad " + n + " Category",
                                                            categoryNames(), defCat[i]));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pad" + n + "_snap", 1 }, "Pad " + n + " Snap",
                                                            snapNames(), defSnap[i]));
    }

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };

    addF ("crush_bits", "Bit Depth",       { 4.f, 16.f, 1.f }, 16.f);
    addF ("crush_rate", "Sample Rate",     { 4000.f, 48000.f, 1.f, 0.5f }, 48000.f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sat_type", 1 }, "Saturation Type",
                                                        StringArray { "Off", "Warm Tube", "Cassette", "Vinyl" }, 0));
    addF ("sat_drive", "Drive",            { 0.f, 1.f, 0.001f }, 0.3f);
    addF ("sat_mix",   "Saturation Mix",   { 0.f, 1.f, 0.001f }, 1.f);
    addF ("lowcut",    "Low Cut",          { 20.f, 500.f, 1.f, 0.5f }, 20.f);
    addF ("highcut",   "High Cut",         { 2000.f, 20000.f, 1.f, 0.5f }, 20000.f);
    addF ("eq_freq",   "EQ Frequency",     { 100.f, 8000.f, 1.f, 0.4f }, 1000.f);
    addF ("eq_gain",   "EQ Gain",          { -12.f, 12.f, 0.1f }, 0.f);
    addF ("eq_q",      "EQ Q",             { 0.3f, 4.f, 0.01f, 0.6f }, 1.f);
    addF ("int_bpm",   "Internal Tempo",   { 60.f, 200.f, 1.f }, 90.f);
    return layout;
}

static std::shared_ptr<Sample> makeDefaultSample (int cat)
{
    const double rate = 44100.0;
    const float lenSec = cat == Kick ? 0.6f : cat == Snare ? 0.35f : cat == HiHat ? 0.09f
                       : cat == OpenHat ? 0.5f : cat == Clap ? 0.35f : cat == Crash ? 1.6f : 0.18f;
    const int n = (int) (rate * lenSec);
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    auto s = std::make_shared<Sample>();
    s->rate = rate;
    s->name = "Default " + categoryNames()[cat];
    s->data.setSize (1, n);
    s->data.clear();
    auto* d = s->data.getWritePointer (0);

    juce::Random r (1234 + cat);
    float phase = 0.f, prev = 0.f;

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) rate;
        const float noise = r.nextFloat() * 2.f - 1.f;
        const float hi = noise - prev; // crude high-pass
        prev = noise;
        float o = 0.f;

        switch (cat)
        {
            case Kick:
            {
                const float f = 45.f + 120.f * std::exp (-t * 35.f);
                phase += twoPi * f / (float) rate;
                o = std::tanh (std::sin (phase) * std::exp (-t * 7.f) * 1.8f);
                break;
            }
            case Snare:   o = 0.55f * hi * std::exp (-t * 20.f) + 0.6f * std::sin (twoPi * 190.f * t) * std::exp (-t * 28.f); break;
            case HiHat:   o = 0.6f * hi * std::exp (-t * 70.f); break;
            case OpenHat: o = 0.5f * hi * std::exp (-t * 9.f); break;
            case Clap:
            {
                float e = 0.f;
                for (int k = 0; k < 3; ++k)
                {
                    const float tt = t - 0.011f * (float) k;
                    if (tt >= 0.f) e += std::exp (-tt * 110.f);
                }
                if (t > 0.03f) e += 0.6f * std::exp (-(t - 0.03f) * 16.f);
                o = 0.4f * (0.5f * noise + 0.5f * hi) * e;
                break;
            }
            case Crash:   o = 0.4f * hi * std::exp (-t * 2.6f); break;
            default:
            {
                const float f = 380.f + 300.f * std::exp (-t * 40.f);
                phase += twoPi * f / (float) rate;
                o = 0.7f * std::sin (phase) * std::exp (-t * 26.f);
                break;
            }
        }
        d[i] = o;
    }
    return s;
}

//==============================================================================
BoomBapProcessor::BoomBapProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();

    for (int p = 0; p < kPads; ++p)
    {
        const juce::String n (p + 1);
        volParam[p]  = apvts.getRawParameterValue ("pad" + n + "_vol");
        catParam[p]  = apvts.getRawParameterValue ("pad" + n + "_cat");
        snapParam[p] = apvts.getRawParameterValue ("pad" + n + "_snap");
        uiTrigger[p].store (false);
        for (int s = 0; s < kSteps; ++s) pattern[p][s].store (0);
        samples[p] = makeDefaultSample ((int) catParam[p]->load());
        sampleName[p] = samples[p]->name;
    }

    fxBits = apvts.getRawParameterValue ("crush_bits");   fxRate = apvts.getRawParameterValue ("crush_rate");
    fxSatType = apvts.getRawParameterValue ("sat_type");  fxDrive = apvts.getRawParameterValue ("sat_drive");
    fxMix = apvts.getRawParameterValue ("sat_mix");       fxLow = apvts.getRawParameterValue ("lowcut");
    fxHigh = apvts.getRawParameterValue ("highcut");      fxEqFreq = apvts.getRawParameterValue ("eq_freq");
    fxEqGain = apvts.getRawParameterValue ("eq_gain");    fxEqQ = apvts.getRawParameterValue ("eq_q");
    intBpm = apvts.getRawParameterValue ("int_bpm");

    // Default boom bap beat: kick, snare 2 & 4, 8th hats, open hat on the "and" of 4.
    pattern[0][0].store (120); pattern[0][7].store (90); pattern[0][10].store (110);
    pattern[1][4].store (120); pattern[1][12].store (120); pattern[1][15].store (40);
    for (int s = 0; s < kSteps; s += 2) pattern[2][s].store (s % 4 == 0 ? 100 : 72);
    pattern[2][14].store (0);
    pattern[3][14].store (100);
}

bool BoomBapProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void BoomBapProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    internalPpq = 0.0;
    for (auto& v : voices) v.active = false;
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset(); lp[c].reset(); peak[c].reset(); satLP[c].reset();
        phase[c] = 0.f; held[c] = 0.f;
    }
    lastLow = lastHigh = lastEqF = lastEqQ = -1.f;
    lastEqG = -99.f;
    lastSat = -1;
}

//==============================================================================
void BoomBapProcessor::addEvent (int sample, int pad, float vel)
{
    if (nEvents < (int) std::size (events))
        events[nEvents++] = { sample, pad, vel };
}

void BoomBapProcessor::fire (const Event& e)
{
    if ((int) catParam[e.pad]->load() == HiHat) // closed hat chokes open hats
        for (int q = 0; q < kPads; ++q)
            if (q != e.pad && (int) catParam[q]->load() == OpenHat) voices[q].active = false;

    const juce::SpinLock::ScopedTryLockType lock (sampleLock);
    if (! lock.isLocked() || samples[e.pad] == nullptr) return;

    auto& v = voices[e.pad];
    v.sample = samples[e.pad];
    v.pos = 0.0;
    v.ratio = v.sample->rate / sr;
    v.gain = e.vel;
    v.active = true;
}

void BoomBapProcessor::scheduleSequencer (int numSamples)
{
    double bpm = 120.0, ppq = 0.0;
    bool havePpq = false, playing = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; playing = pos->getIsPlaying(); }
        }

    if (! havePpq) // standalone / hosts without transport: use the internal clock
    {
        bpm = intBpm->load();
        playing = internalPlay.load();
        if (playing) { ppq = internalPpq; internalPpq += numSamples * bpm / (60.0 * sr); }
        else internalPpq = 0.0;
    }

    if (! playing || bpm <= 0.0) { currentStep.store (-1); return; }

    currentStep.store (((int) std::floor (ppq * 4.0) % kSteps + kSteps) % kSteps);

    const double endPpq = ppq + numSamples * bpm / (60.0 * sr);
    for (int p = 0; p < kPads; ++p)
    {
        const int snap = (int) snapParam[p]->load();
        for (int s = 0; s < kSteps; ++s)
        {
            const int v = pattern[p][s].load();
            if (v <= 0) continue;

            const double t = ((double) s + snapOffset (snap, s)) * 0.25; // position within the 4-beat bar
            const double occ = t + 4.0 * std::ceil ((ppq - t) / 4.0);
            if (occ >= ppq && occ < endPpq)
                addEvent (juce::jlimit (0, numSamples - 1, (int) ((occ - ppq) * 60.0 / bpm * sr)), p, (float) v / 127.f);
        }
    }
}

void BoomBapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    nEvents = 0;

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            const int pad = msg.getNoteNumber() - kFirstNote;
            if (pad >= 0 && pad < kPads) addEvent (juce::jlimit (0, n - 1, meta.samplePosition), pad, msg.getFloatVelocity());
        }
    }
    for (int p = 0; p < kPads; ++p)
        if (uiTrigger[p].exchange (false)) addEvent (0, p, 0.9f);

    scheduleSequencer (n);
    std::sort (events, events + nEvents, [] (const Event& a, const Event& b) { return a.sample < b.sample; });

    float padGain[kPads];
    for (int p = 0; p < kPads; ++p) padGain[p] = juce::Decibels::decibelsToGain (volParam[p]->load(), -60.f) * 0.8f;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int ev = 0;

    for (int i = 0; i < n; ++i)
    {
        while (ev < nEvents && events[ev].sample <= i) fire (events[ev++]);

        float l = 0.f, r = 0.f;
        for (int p = 0; p < kPads; ++p)
        {
            auto& v = voices[p];
            if (! v.active) continue;

            const auto& buf = v.sample->data;
            const int len = buf.getNumSamples();
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; continue; }

            const float f = (float) (v.pos - i0);
            const float* d0 = buf.getReadPointer (0);
            const float* d1 = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : d0;
            const float g = v.gain * padGain[p];
            l += g * (d0[i0] + f * (d0[i0 + 1] - d0[i0]));
            r += g * (d1[i0] + f * (d1[i0 + 1] - d1[i0]));
            v.pos += v.ratio;
        }
        L[i] = l;
        R[i] = r;
    }

    applyFx (buffer);
}

//==============================================================================
void BoomBapProcessor::updateFilters()
{
    using C = juce::dsp::IIR::Coefficients<float>;
    const float lo = fxLow->load();
    const float hi = std::min (fxHigh->load(), (float) (sr * 0.45));
    const float ef = fxEqFreq->load(), eg = fxEqGain->load(), eq = fxEqQ->load();
    const int sat = (int) fxSatType->load();

    if (lo != lastLow)   { lastLow = lo;   auto c = C::makeHighPass (sr, lo); for (auto& f : hp) f.coefficients = c; }
    if (hi != lastHigh)  { lastHigh = hi;  auto c = C::makeLowPass  (sr, hi); for (auto& f : lp) f.coefficients = c; }
    if (ef != lastEqF || eg != lastEqG || eq != lastEqQ)
    {
        lastEqF = ef; lastEqG = eg; lastEqQ = eq;
        auto c = C::makePeakFilter (sr, ef, eq, juce::Decibels::decibelsToGain (eg));
        for (auto& f : peak) f.coefficients = c;
    }
    if (sat != lastSat)
    {
        lastSat = sat;
        const float fc = std::min (sat == 3 ? 8500.f : 11000.f, (float) (sr * 0.45)); // vinyl duller than tape
        auto c = C::makeLowPass (sr, fc);
        for (auto& f : satLP) f.coefficients = c;
    }
}

void BoomBapProcessor::applyFx (juce::AudioBuffer<float>& buf)
{
    updateFilters();
    const int n = buf.getNumSamples();

    const float bits = fxBits->load(), rate = fxRate->load();
    const bool crushOn = bits < 15.5f || rate < (float) sr * 0.99f;
    const float q = std::pow (2.f, bits - 1.f);
    const float phaseInc = (float) (rate / sr);

    const int sat = (int) fxSatType->load();
    const float drive = fxDrive->load(), mix = fxMix->load();
    const float g = juce::Decibels::decibelsToGain (drive * 24.f);
    const float comp = 1.f / std::pow (g, 0.35f);
    const float tubeBias = 0.15f, tubeDc = std::tanh (tubeBias);

    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];

            if (crushOn) // bitcrush + sample-rate reduction (sample & hold)
            {
                phase[ch] += phaseInc;
                if (phase[ch] >= 1.f)
                {
                    phase[ch] -= 1.f;
                    held[ch] = std::round (x * q) / q;
                }
                x = held[ch];
            }

            if (sat > 0)
            {
                float wet = 0.f;
                switch (sat)
                {
                    case 1: wet = (std::tanh (x * g + tubeBias) - tubeDc) * comp; break; // asymmetric = even harmonics
                    case 2: wet = satLP[ch].processSample (std::tanh (x * g * 0.8f)) * comp;
                            wet += (fxRand.nextFloat() * 2.f - 1.f) * 0.0006f; break;    // tape hiss
                    default: wet = satLP[ch].processSample (std::tanh (x * g * 0.7f)) * comp;
                             wet += (fxRand.nextFloat() * 2.f - 1.f) * 0.0008f;
                             if (fxRand.nextFloat() < 0.0003f) wet += (fxRand.nextFloat() * 2.f - 1.f) * 0.3f; // crackle
                             break;
                }
                x += mix * (wet - x);
            }

            x = hp[ch].processSample (x);
            x = lp[ch].processSample (x);
            x = peak[ch].processSample (x);
            d[i] = x;
        }
    }
}

//==============================================================================
bool BoomBapProcessor::loadSampleFile (int pad, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2) return false;

    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    auto s = std::make_shared<Sample>();
    s->data.setSize (std::min (2, (int) reader->numChannels), len);
    reader->read (&s->data, 0, len, 0, true, true);
    s->rate = reader->sampleRate;
    s->name = file.getFileNameWithoutExtension();

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        samples[pad] = s;
    }
    samplePath[pad] = file.getFullPathName();
    sampleName[pad] = s->name;
    return true;
}

//==============================================================================
Kit BoomBapProcessor::readKit() const
{
    Kit k;
    for (int p = 0; p < kPads; ++p)
    {
        k.category[p] = (int) catParam[p]->load();
        k.snap[p] = (int) snapParam[p]->load();
        for (int s = 0; s < kSteps; ++s) k.pattern[p][s] = pattern[p][s].load();
    }
    return k;
}

void BoomBapProcessor::setChoiceParam (const juce::String& id, int index)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

void BoomBapProcessor::setFloatParam (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void BoomBapProcessor::writeKit (const Kit& k, int onlyPad)
{
    for (int p = 0; p < kPads; ++p)
    {
        if (onlyPad >= 0 && p != onlyPad) continue;
        for (int s = 0; s < kSteps; ++s) pattern[p][s].store (k.pattern[p][s]);
        if ((int) snapParam[p]->load() != k.snap[p])
            setChoiceParam ("pad" + juce::String (p + 1) + "_snap", k.snap[p]);
    }
}

void BoomBapProcessor::rollKit()
{
    auto k = readKit();
    generateKit (k, rng);
    writeKit (k, -1);
}

void BoomBapProcessor::rollPad (int pad)
{
    auto k = readKit();
    rollSingle (k, pad, rng);
    writeKit (k, -1); // other hats may have been choked, so write all patterns
}

void BoomBapProcessor::clearPad (int pad)
{
    for (int s = 0; s < kSteps; ++s) pattern[pad][s].store (0);
}

void BoomBapProcessor::applyFxPreset (int preset)
{
    struct P { float bits, rate; int sat; float drive, mix, low, high, ef, eg, eq; };
    static const P presets[] = {
        { 12.f, 26040.f, 1, 0.35f, 1.0f,  30.f,  9500.f, 3000.f,  0.f, 1.f }, // SP-1200
        { 14.f, 32000.f, 3, 0.40f, 0.8f,  60.f,  8000.f, 1000.f,  0.f, 1.f }, // Dusty Vinyl
        { 16.f, 48000.f, 2, 0.50f, 1.0f,  40.f, 11000.f,  200.f,  2.f, 0.8f }, // Tape Deck
        { 16.f, 48000.f, 0, 0.30f, 1.0f,  20.f, 20000.f, 1000.f,  0.f, 1.f }  // Reset
    };
    const auto& p = presets[juce::jlimit (0, 3, preset)];
    setFloatParam ("crush_bits", p.bits);  setFloatParam ("crush_rate", p.rate);
    setChoiceParam ("sat_type", p.sat);    setFloatParam ("sat_drive", p.drive);
    setFloatParam ("sat_mix", p.mix);      setFloatParam ("lowcut", p.low);
    setFloatParam ("highcut", p.high);     setFloatParam ("eq_freq", p.ef);
    setFloatParam ("eq_gain", p.eg);       setFloatParam ("eq_q", p.eq);
}

//==============================================================================
void BoomBapProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int p = 0; p < kPads; ++p)
    {
        juce::StringArray steps;
        for (int s = 0; s < kSteps; ++s) steps.add (juce::String (pattern[p][s].load()));
        state.setProperty ("pattern" + juce::String (p), steps.joinIntoString (","), nullptr);
        state.setProperty ("samplePath" + juce::String (p), samplePath[p], nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void BoomBapProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    for (int p = 0; p < kPads; ++p)
    {
        const auto steps = juce::StringArray::fromTokens (state.getProperty ("pattern" + juce::String (p)).toString(), ",", "");
        for (int s = 0; s < kSteps && s < steps.size(); ++s) pattern[p][s].store (steps[s].getIntValue());

        const juce::File f (state.getProperty ("samplePath" + juce::String (p)).toString());
        if (f.existsAsFile()) loadSampleFile (p, f);
    }
}

juce::AudioProcessorEditor* BoomBapProcessor::createEditor() { return new BoomBapEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BoomBapProcessor(); }
