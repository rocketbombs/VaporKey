# VaporKey

A CPU-efficient wavetable synthesizer VST3 built with JUCE, designed for vaporwave and synthwave production. VaporKey combines clean band-limited oscillators with a dedicated "analog warmth" section and a full FX chain — all wrapped in a neon-lit aesthetic with a perspective grid, scan-lined sun, and glowing knobs.

Targeted at FL Studio and other VST3 hosts on Windows, macOS, and Linux.

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

Ten factory presets ship with VaporKey:

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

In addition to factory presets, VaporKey supports **user presets** that are saved to and loaded from your system's user application data directory (`RocketBombs/VaporKey/Presets`). User presets can be saved, renamed, and deleted from within the plugin UI.

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

## Installing in FL Studio (Windows)

1. Download `VaporKey-Windows-VST3` from the latest GitHub Actions run.
2. Copy `VaporKey.vst3` into `C:\Program Files\Common Files\VST3\` (or any folder FL Studio scans).
3. In FL Studio: **Options → Manage plugins → Find more plugins**. VaporKey appears under *Generators*.

---

## License

MIT — see source file headers.
