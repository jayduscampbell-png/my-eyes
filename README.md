# Boom Bap Drums (JUCE VST3 / AU / Standalone)

8-pad drum instrument with a built-in 16-step programmer, category-aware 90s boom bap groove generators,
per-pad swing snapping, and a vintage FX rack.

## Build

Requires CMake 3.22+, a C++17 compiler, and internet access on first configure (JUCE is fetched).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The VST3 is copied to your system plugin folder after the build (`COPY_PLUGIN_AFTER_BUILD`).
Windows: Visual Studio 2022. macOS: Xcode command line tools (AU is built too). Linux: JUCE's usual
dependencies (libasound2-dev, libx11-dev, libfreetype-dev, etc.).

## Windows 11 + FL Studio

Option A, no local setup: push this folder to a GitHub repo, open the Actions tab, run "Build Windows VST3",
then download the `BoomBapDrums-VST3-Windows` artifact.

Option B, local: install Visual Studio 2022 (Desktop development with C++) and CMake, then in a
"x64 Native Tools Command Prompt":

    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target BoomBapDrums_VST3

Install: copy the `Boom Bap Drums.vst3` folder to `C:\Program Files\Common Files\VST3\`
(Option B does this automatically if the prompt is run as administrator).

In FL Studio: Options > Manage plugins > Find installed plugins (make sure that folder is in the scan list),
then add "Boom Bap Drums" from the Plugin database > Installed > Generators. Pads respond to MIDI notes 36-43,
so FL's piano roll C1-G1 plays them. The sequencer follows FL's playback.

## Use

- Pads respond to MIDI notes 36-43 (MPK Mini MK3 pad bank A, C1-G1). Click a pad to audition it.
- Drag a WAV/AIFF/FLAC onto a pad to replace its sample. Right-click a pad for volume knob, category,
  snap, roll groove, clear, and load sample.
- Click steps to cycle: normal -> accent -> ghost -> off.
- Each row has a Category and Snap dropdown plus a **Roll** button that generates a groove for that sound
  that fits around the other pads. **Roll 8-Pad Groove** regenerates the whole kit and picks a matching swing feel.
- Snap modes: Straight 16ths, MPC 58% Swing, 8th Triplets, Dilla Late Push.
- Sequencer follows your DAW transport. In Standalone (or hosts without transport) use Internal Play + tempo.
- Closed hats choke open hats.
- FX Rack tab: bit depth, sample rate, Warm Tube / Cassette / Vinyl saturation, low cut, high cut,
  parametric EQ, and presets (SP-1200, Dusty Vinyl, Tape Deck, Reset).

## Notes

- Written without access to a compiler or DAW, so expect to fix a few compile errors on the first build.
- Pad outputs are summed to one stereo bus (no per-pad outs or pan yet).
