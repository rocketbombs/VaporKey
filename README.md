# VaporKey

A wavetable synthesizer plugin (VST3 + Standalone) built with [JUCE 8](https://juce.com/), aimed at vaporwave, synthwave, and lo-fi production.

> **Status: In development.** VaporKey is pre-release software. Features, parameters, and preset formats may change without notice, and saved sessions are not guaranteed to load across versions.

---

## Overview

VaporKey is a 16-voice subtractive/wavetable hybrid synth with a built-in arpeggiator, four assignable macros, and an integrated FX chain. It runs as a VST3 plugin or as a standalone application on Windows, macOS, and Linux.

## Features

**Oscillators**
- Three wavetable oscillators with nine factory banks (Basic, Saws, Squares, Vocal, Bell, Digital, Harmonic, Glass, Reso), 8 frames each, mip-mapped across 10 octaves
- Per-oscillator level, pan, coarse/fine tuning, unison (1–7), detune, and phase
- Drag-and-drop `.wav` import to use custom wavetables
- Sub oscillator (sine / square / triangle, –1 or –2 octaves)
- Noise generator (white / pink / brown)

**Filter & Modulation**
- State-variable TPT filter (LP / BP / HP) with cutoff, resonance, drive, key tracking, and velocity
- Amp ADSR, Mod ADSR (routed to filter), and a decay-only pitch envelope (±24 semitones)
- Two LFOs with free-run or tempo-synced rates and selectable shapes
- Four assignable macro knobs covering filter, oscillator, LFO, and FX destinations

**Arpeggiator**
- Modes: Up, Down, Up/Down, Down/Up, As Played, Random
- Tempo-synced rate, 1–4 octave range, gate, swing, and latch

**Analog Warmth**
- *Grit* — per-sample phase jitter
- *Vibe* — background hiss
- *Drift* — slow, decorrelated per-oscillator pitch drift
- *Sat* — soft tanh saturation on the master bus

**FX Chain** (fixed routing)
1. Distortion (Soft / Hard / Fold / Bit)
2. 3-band EQ (low shelf, parametric mid, high shelf)
3. Chorus
4. Phaser
5. Stereo ping-pong delay (free or tempo-synced)
6. Plate reverb
7. Compressor
8. Master gain & stereo width

**Voicing & MIDI**
- 16-voice polyphony, plus mono / legato / glide modes
- Configurable pitch-bend range (1–24 semitones)
- Mod wheel and channel aftertouch as modulation sources

**Presets**
- Factory presets stored in `Source/Presets.json` and embedded at build time, organized by category (Bass, Lead, Pad, Pluck, Keys, Bell, FX, Arp)
- User presets saved under the system app-data directory (`RocketBombs/VaporKey/Presets`)

---

## Building from Source

**Requirements:** CMake 3.22+, a C++17 compiler. JUCE 8.0.4 is fetched automatically by CMake.

```bash
git clone https://github.com/rocketbombs/VaporKey.git
cd VaporKey
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VaporKey_VST3 --parallel
```

Output: `build/VaporKey_artefacts/Release/VST3/VaporKey.vst3`. A `VaporKey_Standalone` target is also produced.

### macOS (universal binary)

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release --target VaporKey_VST3 --parallel
```

### Linux dependencies (Debian / Ubuntu)

```bash
sudo apt-get install libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev libgtk-3-dev
```

### Install paths

| Platform | VST3 install path |
|----------|-------------------|
| Windows  | `C:\Program Files\Common Files\VST3\` |
| macOS    | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux    | `~/.vst3/` |

On macOS, you may need to clear the quarantine attribute after copying:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VaporKey.vst3
```

---

## Project Layout

```
Source/
  PluginProcessor.{h,cpp}   audio processor, parameter tree, FX chain
  PluginEditor.{h,cpp}      UI / editor
  SynthVoice.{h,cpp}        per-voice synthesis
  Wavetable.{h,cpp}         mip-mapped wavetable oscillator
  LookAndFeel.{h,cpp}       custom JUCE LookAndFeel
  Presets.{h,cpp}           factory preset loader
  Presets.json              factory preset data (embedded at build)
CMakeLists.txt
```

## Contributing

The project is in active development and not yet accepting external contributions on a defined schedule. Bug reports and feedback are welcome via the issue tracker.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for release notes.

## License

MIT — see [LICENSE](LICENSE).
