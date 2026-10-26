#pragma once
#include <JuceHeader.h>
#include <vector>

// Category-aware 90s boom bap groove generation + per-pad swing "snap" modes.
// Steps are 16ths: 0 = beat 1, 4 = beat 2 (snare), 8 = beat 3, 12 = beat 4 (snare).
namespace bb
{
constexpr int kPads = 8;
constexpr int kSteps = 16;
constexpr int kFirstNote = 36; // MPK Mini MK3 pad bank A: MIDI 36-43 (C1-G1)

enum Category { Kick = 0, Snare, HiHat, OpenHat, Clap, Crash, Perc, NumCategories };
enum Snap { Straight = 0, MPC58, Triplet, DillaLate, NumSnaps };

inline juce::StringArray categoryNames() { return { "Kick", "Snare", "Hi-Hat", "Open Hat", "Clap", "Crash", "Perc" }; }
inline juce::StringArray snapNames()     { return { "Straight 16ths", "MPC 58% Swing", "8th Triplets", "Dilla Late Push" }; }

// Timing offset in fractions of a 16th step (positive = late).
inline double snapOffset (int snap, int step)
{
    const bool offbeat = (step & 1) != 0;
    switch (snap)
    {
        case MPC58:     return offbeat ? 0.16 : 0.0;   // 58:42 swing ratio on 16ths
        case Triplet:   return offbeat ? 0.333 : 0.0;  // 66.7% shuffle
        case DillaLate: return offbeat ? 0.20 : 0.08;  // everything sits behind the grid
        default:        return 0.0;
    }
}

struct Kit
{
    int pattern[kPads][kSteps] {};   // velocity 0-127, 0 = off
    int category[kPads] {};
    int snap[kPads] {};
};

namespace detail
{
    inline int vel (juce::Random& r, int lo, int hi) { return lo + r.nextInt (hi - lo + 1); }

    // Is a backbone hit (snare/clap, optionally kick) already on this step?
    inline bool taken (const Kit& k, int step, int self, bool includeKick)
    {
        for (int p = 0; p < kPads; ++p)
        {
            if (p == self || k.pattern[p][step] == 0) continue;
            const int c = k.category[p];
            if (c == Snare || c == Clap || (includeKick && c == Kick)) return true;
        }
        return false;
    }

    inline bool sameCategoryTaken (const Kit& k, int step, int self)
    {
        for (int p = 0; p < kPads; ++p)
            if (p != self && k.category[p] == k.category[self] && k.pattern[p][step] > 0) return true;
        return false;
    }
}

// Open hats choke the closed hat on the same step so they don't stack.
inline void chokeHats (Kit& k)
{
    for (int o = 0; o < kPads; ++o)
        if (k.category[o] == OpenHat)
            for (int s = 0; s < kSteps; ++s)
                if (k.pattern[o][s] > 0)
                    for (int h = 0; h < kPads; ++h)
                        if (k.category[h] == HiHat) k.pattern[h][s] = 0;
}

inline void generatePad (Kit& k, int pad, juce::Random& r)
{
    using namespace detail;
    int* pat = k.pattern[pad];
    for (int s = 0; s < kSteps; ++s) pat[s] = 0;

    switch (k.category[pad])
    {
        case Kick:
        {
            static const std::vector<std::vector<int>> templates {
                { 0, 10 }, { 0, 7, 10 }, { 0, 6, 10 }, { 0, 3, 10 },
                { 0, 5, 10, 11 }, { 0, 10, 14 }, { 0, 2, 10 }, { 0, 7, 8, 10 } };
            const auto& t = templates[(size_t) r.nextInt ((int) templates.size())];
            for (int s : t)
                if (! taken (k, s, pad, false)) pat[s] = (s == 0 ? vel (r, 112, 127) : vel (r, 85, 118));
            if (r.nextFloat() < 0.3f)
            {
                const int s = r.nextBool() ? 14 : 15; // turnaround pickup
                if (pat[s] == 0 && ! taken (k, s, pad, false)) pat[s] = vel (r, 60, 85);
            }
            if (pat[0] == 0) pat[0] = 120;
            break;
        }
        case Snare:
        {
            pat[4] = vel (r, 112, 127);
            pat[12] = vel (r, 112, 127);
            static const int ghosts[] = { 2, 7, 9, 11, 14, 15 };
            const int n = r.nextInt (3); // 0-2 ghost notes
            for (int i = 0; i < n; ++i)
            {
                const int s = ghosts[r.nextInt (6)];
                if (pat[s] == 0 && ! taken (k, s, pad, true)) pat[s] = vel (r, 28, 52);
            }
            break;
        }
        case Clap:
            pat[12] = vel (r, 95, 115);
            if (r.nextFloat() < 0.7f) pat[4] = vel (r, 85, 105);
            break;

        case HiHat:
        {
            const int style = r.nextInt (4);
            for (int s = 0; s < kSteps; ++s)
            {
                const bool beat = s % 4 == 0, eighth = s % 2 == 0;
                switch (style)
                {
                    case 0: if (eighth) pat[s] = beat ? vel (r, 98, 112) : vel (r, 68, 85); break;          // straight 8ths
                    case 1: pat[s] = beat ? vel (r, 98, 110) : eighth ? vel (r, 72, 88) : vel (r, 30, 48); break; // 16ths + ghosts
                    case 2: if (eighth && r.nextFloat() > 0.18f) pat[s] = beat ? vel (r, 98, 110) : vel (r, 66, 82); break; // SP-1200 skips
                    default: if (s % 4 == 2) pat[s] = vel (r, 88, 102); else if (s % 8 == 0) pat[s] = vel (r, 55, 70); break; // offbeat pulse
                }
            }
            if (style == 2 && r.nextBool()) pat[r.nextBool() ? 15 : 13] = vel (r, 40, 60);
            break;
        }
        case OpenHat:
        {
            static const int spots[] = { 6, 10, 14 };
            pat[spots[r.nextInt (3)]] = vel (r, 92, 110);
            if (r.nextFloat() < 0.2f) pat[spots[r.nextInt (3)]] = vel (r, 80, 100);
            break;
        }
        case Crash:
            if (r.nextFloat() < 0.35f) pat[0] = vel (r, 100, 120);
            break;

        default: // Perc
        {
            static const int spots[] = { 3, 5, 7, 9, 11, 13, 15, 2, 6, 14 };
            const int count = 2 + r.nextInt (2);
            for (int i = 0, tries = 0; i < count && tries < 40; ++tries)
            {
                const int s = spots[r.nextInt (10)];
                if (pat[s] == 0 && ! taken (k, s, pad, true) && ! sameCategoryTaken (k, s, pad))
                {
                    pat[s] = vel (r, 55, 88);
                    ++i;
                }
            }
            break;
        }
    }
}

inline int randomBaseSwing (juce::Random& r)
{
    const float x = r.nextFloat();
    return x < 0.45f ? MPC58 : x < 0.65f ? DillaLate : x < 0.80f ? Triplet : Straight;
}

// Picks a swing for a sound that sits well with the kit's base feel.
inline int pickSnapFor (int cat, int base, juce::Random& r)
{
    switch (cat)
    {
        case HiHat:
        case OpenHat: return base;
        case Perc:    return r.nextFloat() < 0.6f ? base : (int) Straight;
        case Kick:    return r.nextFloat() < 0.7f ? (base == Triplet ? (int) MPC58 : base) : (int) Straight;
        case Snare:
        case Clap:    return r.nextFloat() < 0.6f ? (int) Straight : (int) DillaLate;
        default:      return Straight;
    }
}

inline int currentBaseSwing (const Kit& k, int ignorePad, juce::Random& r)
{
    for (int p = 0; p < kPads; ++p)
        if (p != ignorePad && k.category[p] == HiHat) return k.snap[p];
    return randomBaseSwing (r);
}

// Full-kit roll: backbone first (snare/clap/kick) so everything else locks around it.
inline void generateKit (Kit& k, juce::Random& r)
{
    static const int order[] = { Snare, Clap, Kick, HiHat, OpenHat, Crash, Perc };
    for (int p = 0; p < kPads; ++p)
        for (int s = 0; s < kSteps; ++s) k.pattern[p][s] = 0;

    for (int c : order)
        for (int p = 0; p < kPads; ++p)
            if (k.category[p] == c) generatePad (k, p, r);

    chokeHats (k);
    const int base = randomBaseSwing (r);
    for (int p = 0; p < kPads; ++p) k.snap[p] = pickSnapFor (k.category[p], base, r);
}

// Single-pad roll that respects what the other pads are already doing.
inline void rollSingle (Kit& k, int pad, juce::Random& r)
{
    generatePad (k, pad, r);
    chokeHats (k);
    k.snap[pad] = pickSnapFor (k.category[pad], currentBaseSwing (k, pad, r), r);
}
} // namespace bb
