# VaporKey

[![Build VaporKey](https://github.com/rocketbombs/vaporkey/actions/workflows/build.yml/badge.svg)](https://github.com/rocketbombs/vaporkey/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)](#installing)

A CPU-efficient wavetable synthesizer VST3 built with JUCE, designed for vaporwave and synthwave production. VaporKey combines clean band-limited oscillators with a dedicated "analog warmth" section and a full FX chain — all wrapped in a neon-lit aesthetic with a perspective grid, scan-lined sun, and glowing knobs.

Targeted at FL Studio and other VST3 hosts on Windows, macOS, and Linux.

> **Highlights** — 3 wavetable oscillators with mip-mapped band-limiting, sub + noise, state-variable filter, two ADSRs + decay-only pitch envelope, two LFOs (free or tempo-synced), four assignable macros, tempo-synced arpeggiator, drag-and-drop custom `.wav` wavetables, full FX chain, and **57 factory presets** in 8 browsable categories.

---

## Feature Overview

### Oscillators

VaporKey has three independent wavetable oscillators plus a sub oscillator and a noise generator.

**Wavetable oscillators (×3)**

Each oscillator draws from one of nine factory wavetable banks:

| Bank | Character |
|------|-----------|
| Basic | Sine → saw → square progression |
| Saws | Harmonic saw variants |
| Squares | Pulse-width family |
| Vocal | Formant / vowel shapes |
| Bell | Inharmonic bell partials |
| Digital | Aliased / bit-reduced textures |
| Harmonic | Additive harmonic series |
| Glass | Glassy, pure overtone stacks |
| Reso | Resonant/comb-filtered timbres |

The **Position** knob morphs continuously between the 8 frames within the selected bank. Per-oscillator controls:

- **On/Off** — bypass the oscillator entirely
- **Shape** — select the wavetable bank
- **Position** — morph through the bank's frames
- **Level** (dB) and **Pan**
- **Coarse** (semitones) and **Fine** (cents) tuning
- **Unison** — stack 1–7 detuned voices per oscillator
- **Detune** — spread amount across unison voices
- **Phase** — fixed start phase or free-running

All wavetables are **mip-mapped** (10 octave levels, 2048 samples per frame) to eliminate aliasing across the keyboard.

**Sub oscillator**

A simple sub underneath the main oscillators, selectable as Sine, Square, or Triangle, tunable one or two octaves below. Has its own level control.

**Noise generator**

Switchable White, Pink, or Brown noise with a dedicated level knob. Useful for breath, texture, or layering into pads.

---

### Filter

A **state-variable TPT filter** (Topology-Preserving Transform) switchable between Low-pass, Band-pass, and High-pass modes.

- **Cutoff** and **Resonance**
- **Drive** — pre-filter saturation that pushes the filter into self-oscillation territory
- **Mod Env Amount** — scales how much the mod envelope opens or closes the filter
- **Key Tracking** — scales cutoff with MIDI note (0 = fixed, 1 = full 1:1 tracking)
- **Filter Velocity** — scales the mod-env amount with note velocity

---

### Envelopes

**Amp envelope (ADSR)**
Controls volume shape. A velocity sensitivity knob scales how strongly note velocity affects the envelope peak.

**Mod envelope (ADSR)**
Assignable envelope primarily routed to filter cutoff (via the Mod Env Amount knob). Also has a velocity sensitivity control.

**Pitch envelope**
A fast decay-only envelope for percussive pitch plucks and kicks. Amount is ±24 semitones; Decay sets how quickly it returns to the base pitch.

---

### LFOs

Two independent LFOs, each with:

- **Shape** — Sine, Triangle, Saw+, Saw−, Square, or Sample & Hold
- **Rate** — free-running Hz rate
- **Sync** — lock rate to host BPM with selectable divisions: 1/32, 1/16, 1/8, 1/4D, 1/4, 1/2, 1/1, 2/1
- **Amount**

LFO1 is primarily routed to filter cutoff; LFO2 to wavetable position.

---

### Macro Modulation

Four freely assignable macro knobs let you map a single control to any modulation destination. Available destinations:

- Filter Cutoff / Resonance
- Oscillator 1/2/3 Position, Level, or Detune
- LFO 1/2 Rate
- Delay / Reverb / Chorus / Phaser Mix
- Distortion Drive
- Stereo Width

Each macro has its own Amount knob to scale the modulation depth.

---

### Analog Warmth

A four-knob section that adds organic imperfection to the sound:

| Control | Effect |
|---------|--------|
| **Grit** | Per-sample random phase jitter on the oscillators — adds subtle crunch and movement |
| **Vibe** | Low-level background hiss blended into the signal — simulates tape or circuit noise |
| **Drift** | Slow, decorrelated pitch drift on each oscillator independently — like a vintage polysynth warming up |
| **Sat** | Soft tanh waveshaping across the full mix — adds harmonic density and gentle limiting |

---

### Polyphony & MIDI

- **16-voice polyphony**
- **Mono mode** with configurable **Legato** (envelopes do not retrigger on held notes)
- **Glide** (portamento) with a 0–2 second time range, applied in mono and legato mode
- **Pitch bend** with a configurable range of 1–24 semitones
- Mod wheel and channel aftertouch are available as modulation sources

---

### Arpeggiator

A built-in tempo-synced arpeggiator sits in front of the synth voice. Held notes are intercepted and replayed as a stepped sequence; CCs, pitch bend, and aftertouch pass through untouched.

- **Mode** — Up, Down, Up/Down, Down/Up, As Played, or Random
- **Rate** — host-synced division (1/32 through 2/1, including 1/4D)
- **Octaves** — 1–4 octaves of range
- **Gate** — note length per step (5%–100% of the step time)
- **Swing** — 0–50% offset on every other step
- **Latch** — held notes stay armed after release; pressing a new chord replaces the latch buffer

The arpeggiator follows the host transport, so step timing locks to the project tempo and stays aligned across loops and tempo changes.

---

### Custom Wavetables

Any oscillator can load a user `.wav` file as its wavetable in addition to the nine factory banks.

- **Drag a `.wav` file** onto the wavetable display in the Oscillator page, or
- **Right-click the display** and choose *Load .wav file…* to browse

The audio is sliced into 8 frames, mip-mapped to 10 octave levels (2048 samples per frame) for alias-free playback, and selectable per oscillator alongside the factory shapes. Right-click → *Clear custom wavetable* reverts to factory shapes. Custom wavetables are stored inside the patch state, so saved presets and host sessions reload them automatically.

---

### FX Chain

The post-synthesis FX chain runs in this order:

1. **Distortion** — Soft (tanh), Hard (clip), Fold (wavefold), or Bit (bit-reduction) modes with Drive and wet/dry Mix
2. **3-Band EQ** — Low shelf, parametric Mid (with frequency control), High shelf
3. **Chorus** — Rate and Depth
4. **Phaser** — Rate, Depth, and Feedback
5. **Stereo Ping-Pong Delay** — Time (or tempo-synced division), Feedback, wet Mix
6. **Plate Reverb** — Size, Damping, wet Mix
7. **Compressor** — Threshold, Ratio, Attack, Release, Makeup Gain
8. **Master** — overall Gain and stereo Width

---

### Presets

VaporKey ships with **57 factory presets** organised into eight browsable categories. The preset browser lets you filter by category and step through patches with prev/next arrows.

| Category | Count | Highlights |
|----------|------:|------------|
| **Bass** | 8 | Neon Bass, Sub Hammer, Reese Bass, Acid 303, Glide Bass, FM Bass, Pulse Bass, Wobble |
| **Lead** | 9 | Vapor Lead, Aggro Lead, Sawtooth Hero, Square Wave, Vintage Mono, Resonant Lead, Octave Lead, Bright Lead, Pulse Lead |
| **Pad** | 8 | Synthwave Pad, Strings Pad, Choir Pad, Warm Pad, Glass Pad, Lush Pad, Sweep Pad, Soft Pad |
| **Pluck** | 7 | Pluck, Clean Pluck, FM Pluck, Wood Pluck, Bell Pluck, Soft Pluck, Sharp Pluck |
| **Keys** | 7 + Init | Vapor Keys, Electric Keys, Vintage Stack, Soft Rhodes, Bright Keys, Pure Sine, Mellow Keys |
| **Bell** | 5 | Glass Bell, Crystal Bell, Toy Bell, Tine Bell, Music Box |
| **FX** | 6 | Hover Drone, Riser, Atmosphere, Texture, Whoosh, Sci-Fi Sweep |
| **Arp** | 6 | Arp Stab, Arp Sequence, Arp Pluck, Arp Bass, Arp Pad, Arp Random |

**User presets** save to your system's user application data directory (`RocketBombs/VaporKey/Presets`) and can be saved, renamed, and deleted from within the plugin UI. User presets are listed alongside the factory library in the browser.

---

## Building from Source

**Requirements:** CMake 3.22+, a C++17 compiler. JUCE 8.0.4 is fetched automatically.

```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build only the VST3 (fastest)
cmake --build build --target VaporKey_VST3 --parallel
```

Output: `build/VaporKey_artefacts/Release/VST3/VaporKey.vst3`

A **Standalone** build target (`VaporKey_Standalone`) is also available if you want to run VaporKey outside a DAW.

### macOS (universal binary)

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release --target VaporKey_VST3 --parallel
```

### Linux dependencies (Ubuntu/Debian)

```bash
sudo apt-get install libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev libgtk-3-dev
```

---

## CI Builds (GitHub Actions)

Every push to `main` or `claude/**` branches triggers builds on all three platforms. Artifacts are uploaded to the Actions run:

| Artifact | Platform |
|----------|----------|
| `VaporKey-Windows-VST3` | Windows (VS 2022, x64) |
| `VaporKey-macOS-VST3` | macOS 14 (universal x86_64 + arm64) |
| `VaporKey-Linux-VST3` | Ubuntu 22.04 |

---

## Installing

Download the VST3 for your platform from the latest [GitHub Actions run](https://github.com/rocketbombs/vaporkey/actions) (or build from source) and copy `VaporKey.vst3` into the standard VST3 location:

| Platform | VST3 path |
|----------|-----------|
| Windows  | `C:\Program Files\Common Files\VST3\` |
| macOS    | `~/Library/Audio/Plug-Ins/VST3/` (user) or `/Library/Audio/Plug-Ins/VST3/` (system) |
| Linux    | `~/.vst3/` (user) or `/usr/lib/vst3/` (system) |

Then rescan plugins in your DAW. In FL Studio: **Options → Manage plugins → Find more plugins**; VaporKey appears under *Generators*. In Ableton, Reaper, Bitwig, Cubase, etc., trigger a plugin rescan from the preferences/settings.

---

## License

VaporKey is released under the [MIT License](LICENSE).

JUCE is fetched at configure time and is licensed separately under JUCE's own dual-license terms (see [juce.com/get-juce](https://juce.com/get-juce/)). Distributing a binary built with JUCE is your responsibility under JUCE's license.
