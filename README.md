# VaporKey

**A CPU-efficient wavetable synthesizer VST3 for vaporwave, synthwave, and lo-fi production.**

VaporKey pairs three band-limited wavetable oscillators with a dedicated *Analog Warmth* section, a built-in arpeggiator, and a full mix-ready FX chain — all wrapped in a neon-lit UI with a perspective grid, scan-lined sun, and glowing knobs. Built with [JUCE 8](https://juce.com/) for FL Studio and any other VST3 host on Windows, macOS, and Linux.

> **Current release:** v0.3 — adds arpeggiator, drag-and-drop user wavetables, expanded category-based preset browser, and tempo-synced LFOs.

---

## Quick Start

### 1. Download

Grab the latest build for your platform from the [**Releases**](https://github.com/rocketbombs/VaporKey/releases) page, or from the most recent successful run on the [**Actions**](https://github.com/rocketbombs/VaporKey/actions) tab:

| Platform | Artifact | Install path |
|----------|----------|--------------|
| **Windows** | `VaporKey-Windows-VST3` | `C:\Program Files\Common Files\VST3\` |
| **macOS** (universal) | `VaporKey-macOS-VST3` | `~/Library/Audio/Plug-Ins/VST3/` |
| **Linux** | `VaporKey-Linux-VST3` | `~/.vst3/` |

### 2. Install

Unzip the artifact and copy `VaporKey.vst3` into the install path above. macOS users may need to right-click → **Open** the first time, or run:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VaporKey.vst3
```

### 3. Scan in your DAW

- **FL Studio** — *Options → Manage plugins → Find more plugins*. VaporKey appears under **Generators**.
- **Ableton / Bitwig / Reaper / Cubase** — rescan VST3s in your plugin preferences.
- **Standalone** — a `VaporKey` standalone executable is also produced if you want to play it without a DAW.

That's it. Load a preset, grab a knob, hit some keys.

---

## Table of Contents

- [Feature Overview](#feature-overview)
  - [Oscillators](#oscillators)
  - [Filter](#filter)
  - [Envelopes](#envelopes)
  - [LFOs](#lfos)
  - [Arpeggiator](#arpeggiator)
  - [Macro Modulation](#macro-modulation)
  - [Analog Warmth](#analog-warmth)
  - [FX Chain](#fx-chain)
  - [Polyphony & MIDI](#polyphony--midi)
  - [Presets](#presets)
- [Building from Source](#building-from-source)
- [CI & Releases](#ci--releases)
- [License](#license)

---

## Feature Overview

### Oscillators

Three independent wavetable oscillators plus a sub oscillator and a noise generator.

**Wavetable oscillators (×3)** — each draws from one of nine factory wavetable banks:

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
| Reso | Resonant / comb-filtered timbres |

The **Position** knob morphs continuously between the 8 frames of the selected bank. Per-oscillator controls:

- **On/Off** — bypass the oscillator entirely
- **Shape** — select wavetable bank
- **Position** — morph through the bank's frames
- **Level** (dB) and **Pan**
- **Coarse** (semitones) and **Fine** (cents) tuning
- **Unison** — stack 1–7 detuned voices per oscillator
- **Detune** — spread amount across unison voices
- **Phase** — fixed start phase or free-running

All wavetables are **mip-mapped** (10 octave levels, 2048 samples per frame) to eliminate aliasing across the keyboard.

**Custom wavetables.** Drag any `.wav` file onto the wavetable display — or right-click → **Load .wav…** — to use it as the active oscillator's table. Files are sliced into 8 frames, normalized, and mip-mapped on import.

**Sub oscillator** — Sine, Square, or Triangle, tunable one or two octaves below, with its own level control.

**Noise generator** — White, Pink, or Brown with a dedicated level knob. Useful for breath, texture, or layering into pads.

---

### Filter

A **state-variable TPT filter** (Topology-Preserving Transform) switchable between Low-pass, Band-pass, and High-pass.

- **Cutoff** and **Resonance**
- **Drive** — pre-filter saturation that pushes the filter into self-oscillation territory
- **Mod Env Amount** — scales how much the mod envelope opens or closes the filter
- **Key Tracking** — scales cutoff with MIDI note (0 = fixed, 1 = full 1:1 tracking)
- **Filter Velocity** — scales the mod-env amount with note velocity

---

### Envelopes

- **Amp envelope (ADSR)** — volume shape, with a velocity-sensitivity knob.
- **Mod envelope (ADSR)** — primarily routed to filter cutoff via the *Mod Env Amount* knob, with its own velocity sensitivity.
- **Pitch envelope** — fast decay-only envelope for percussive plucks and kicks. Amount is ±24 semitones; *Decay* sets how quickly it returns to the base pitch.

---

### LFOs

Two independent LFOs, each with:

- **Shape** — Sine, Triangle, Saw+, Saw−, Square, or Sample & Hold
- **Rate** — free-running Hz
- **Sync** — lock to host BPM with divisions: 1/32, 1/16, 1/8, 1/4D, 1/4, 1/2, 1/1, 2/1
- **Amount**

LFO1 is primarily routed to filter cutoff; LFO2 to wavetable position.

---

### Arpeggiator

A built-in arpeggiator that runs on incoming MIDI before the synth voices.

- **Modes** — Up, Down, Up/Down, Down/Up, As Played, Random
- **Rate** — tempo-synced (1/32 through 2/1, with dotted divisions)
- **Octaves** — 1–4
- **Gate** — note length as a fraction of the step
- **Swing** — 50–75%
- **Latch** — hold notes after release until a new chord is played

---

### Macro Modulation

Four freely assignable macro knobs map a single control to any modulation destination:

- Filter Cutoff / Resonance
- Oscillator 1/2/3 Position, Level, or Detune
- LFO 1/2 Rate
- Delay / Reverb / Chorus / Phaser Mix
- Distortion Drive
- Stereo Width

Each macro has its own *Amount* knob to scale modulation depth.

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

### FX Chain

The post-synthesis FX chain runs in this fixed order:

1. **Distortion** — Soft (tanh), Hard (clip), Fold (wavefold), or Bit (bit-reduction) modes with Drive and wet/dry Mix
2. **3-Band EQ** — Low shelf, parametric Mid (with frequency control), High shelf
3. **Chorus** — Rate and Depth
4. **Phaser** — Rate, Depth, and Feedback
5. **Stereo Ping-Pong Delay** — Time (or tempo-synced division), Feedback, wet Mix
6. **Plate Reverb** — Size, Damping, wet Mix
7. **Compressor** — Threshold, Ratio, Attack, Release, Makeup Gain
8. **Master** — overall Gain and stereo Width

---

### Polyphony & MIDI

- **16-voice polyphony**
- **Mono mode** with configurable **Legato** (envelopes do not retrigger on held notes)
- **Glide** (portamento) with a 0–2 second time range, applied in mono and legato modes
- **Pitch bend** with a configurable range of 1–24 semitones
- Mod wheel and channel aftertouch are available as modulation sources

---

### Presets

Factory presets ship organized into categories (Leads, Pads, Bass, Keys, Plucks, FX) and include:

| Preset | Description |
|--------|-------------|
| Init | Clean single-oscillator starting point |
| Vapor Lead | Detuned dual-osc lead with filter envelope and delay |
| Synthwave Pad | Three-oscillator lush pad with slow attack and heavy reverb |
| Neon Bass | Tight bass with sub oscillator and driven filter |
| Pluck | Short-attack pluck with pitch envelope pop |
| Glass Bell | Inharmonic bell with long decay and shimmer reverb |
| Aggro Lead | Distorted, wide seven-voice lead with phaser |
| Wobble | LFO-driven wub bass with sub oscillator |
| Vapor Keys | Clean electric-piano-style keys with chorus and delay |
| Hover Drone | Evolving, fully detuned ambient drone |

…plus more shipped in the v0.3 expanded category browser.

**User presets** are saved to your system's user application data directory (`RocketBombs/VaporKey/Presets`) and can be saved, renamed, and deleted from within the plugin UI.

---

## Building from Source

**Requirements:** CMake 3.22+, a C++17 compiler. JUCE 8.0.4 is fetched automatically by CMake — no separate JUCE install needed.

```bash
git clone https://github.com/rocketbombs/VaporKey.git
cd VaporKey

# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the VST3 (fastest)
cmake --build build --target VaporKey_VST3 --parallel
```

Output: `build/VaporKey_artefacts/Release/VST3/VaporKey.vst3`

A **Standalone** target (`VaporKey_Standalone`) is also available if you want to run VaporKey outside a DAW.

### macOS (universal binary)

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release --target VaporKey_VST3 --parallel
```

### Linux dependencies (Ubuntu / Debian)

```bash
sudo apt-get install libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev libgtk-3-dev
```

---

## CI & Releases

Every push to `main` or `claude/**` and every `v*` tag triggers builds on all three platforms. Artifacts are uploaded to the run:

| Artifact | Platform |
|----------|----------|
| `VaporKey-Windows-VST3` | Windows (VS 2022, x64) |
| `VaporKey-macOS-VST3` | macOS 14 (universal x86_64 + arm64) |
| `VaporKey-Linux-VST3` | Ubuntu 22.04 |

Tagged releases also publish to the [Releases](https://github.com/rocketbombs/VaporKey/releases) page.

---

## License

MIT — see source file headers.
