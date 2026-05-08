# VaporKey

A 16-voice wavetable synthesizer (VST3 + Standalone) built on [JUCE 8](https://juce.com/),
tuned for vaporwave, synthwave, and lo-fi production. Three wavetable
oscillators, two LFOs, four assignable macros, an arpeggiator, and a fixed
FX chain — wrapped in a single audio-reactive editor.

> **Status: pre-release (0.4.x).** Parameters, preset format, and saved-state
> compatibility may change without notice between minor versions. See
> [CHANGELOG.md](CHANGELOG.md) for what landed most recently.

---

## At a glance

- **VST3** plugin and **Standalone** application
- **Windows / macOS (universal) / Linux**
- **16-voice** polyphony with optional mono / legato / glide
- **57 factory presets** across Bass, Lead, Pad, Pluck, Keys, Bell, FX, Arp
- **Realtime-safe** audio path — no allocations, locks, or I/O in `processBlock`
  (see [docs/RealtimeSafety.md](docs/RealtimeSafety.md))
- **Validated** on every push with [Tracktion pluginval](https://github.com/Tracktion/pluginval)
  at strictness 5

## Synth engine

**Oscillators**
- Three wavetable oscillators, mip-mapped across 10 octaves, with nine factory
  banks of 8 frames each: *Basic, Saws, Squares, Vocal, Bell, Digital,
  Harmonic, Glass, Reso*
- Per-osc level, pan, coarse/fine tuning, unison (1–7), detune, phase
- Drag-and-drop `.wav` import for custom wavetables
- Sub oscillator (sine / square / triangle, –1 or –2 octaves)
- Noise generator (white / pink / brown), per-channel for genuine stereo

**Filter & envelopes**
- State-variable TPT filter (LP / BP / HP) with cutoff, resonance, drive,
  key tracking, and velocity
- Amp ADSR, Mod ADSR (routes to filter), and a decay-only pitch envelope
  (±24 semitones)

**Modulation**
- Two LFOs — free-running or tempo-synced, with multiple shapes
- Four assignable macros covering filter, oscillator, LFO, and FX destinations
- **Mod wheel** and **channel aftertouch** as first-class modulation sources,
  each with its own destination + amount

**Arpeggiator**
- Up, Down, Up/Down, Down/Up, As Played, Random
- Tempo-synced rate, 1–4 octave range, gate, swing, latch

**Analog warmth**
- *Grit* — per-sample phase jitter
- *Vibe* — background hiss
- *Drift* — slow, decorrelated per-oscillator pitch drift
- *Sat* — soft tanh saturation on the master bus

**FX chain** *(fixed routing)*
1. Distortion (Soft / Hard / Fold / Bit)
2. 3-band EQ (low shelf, parametric mid, high shelf)
3. Chorus
4. Phaser
5. Stereo ping-pong delay (free or tempo-synced)
6. Plate reverb
7. Compressor
8. Master gain & stereo width

**MIDI**
- Configurable pitch-bend range (1–24 semitones)
- Mod wheel and aftertouch routed through the same per-block mod sum as the macros

**Presets**
- Factory presets live in [`Source/Presets.json`](Source/Presets.json) and
  are baked into the binary at build time — fork the JSON to add your own.
- User presets are saved under the system app-data directory:
  `RocketBombs/VaporKey/Presets`.

---

## Install

### Prebuilt (recommended)

Every push builds and validates VST3 + Standalone for Windows, macOS, and
Linux. Grab the latest from the
[Actions tab](https://github.com/rocketbombs/VaporKey/actions) or, for
tagged releases, the [Releases page](https://github.com/rocketbombs/VaporKey/releases).

| Platform | Drop the `.vst3` here |
|----------|-----------------------|
| Windows  | `C:\Program Files\Common Files\VST3\` |
| macOS    | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux    | `~/.vst3/` |

On macOS, clear the quarantine attribute after copying so the host will load it:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VaporKey.vst3
```

### Build from source

**Requirements:** CMake 3.22+, a C++17 compiler. JUCE 8.0.4 is fetched
automatically by CMake.

```bash
git clone https://github.com/rocketbombs/VaporKey.git
cd VaporKey
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Outputs:

- `build/VaporKey_artefacts/Release/VST3/VaporKey.vst3`
- `build/VaporKey_artefacts/Release/Standalone/VaporKey[.exe|.app]`

#### macOS (universal binary)

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release --parallel
```

#### Linux dependencies (Debian / Ubuntu)

```bash
sudo apt-get install libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev libgtk-3-dev
```

---

## Project layout

```
Source/
  PluginProcessor.{h,cpp}    AudioProcessor — owns APVTS, engine, FX, presets
  PluginEditor.{h,cpp}       editor shell, page switcher, audio-reactive paint loop
  Parameters.{h,cpp}         APVTS layout + cached atomic pointers (SynthParams)
  SynthEngine.{h,cpp}        voices, MIDI filter, macro/mod-wheel/aftertouch sum
  SynthVoice.{h,cpp}         per-voice synthesis (oscs, sub, noise, filter, envs)
  Wavetable.{h,cpp}          mip-mapped wavetable oscillator
  WavetableImport.{h,cpp}    drag-and-drop .wav -> custom table
  Arpeggiator.{h,cpp}        MIDI-rewriting arp, tempo-synced
  FxChain.{h,cpp}            distortion / EQ / chorus / phaser / delay / reverb / comp / width
  PresetStore.{h,cpp}        factory + user preset save/load/rename
  Presets.{h,cpp}            JSON parser for embedded factory presets
  Presets.json               factory preset data (embedded at build)
  LookAndFeel.{h,cpp}        custom JUCE LookAndFeel
  Pages/                     one source pair per editor page
    OscPage, FilterEnvPage, ModPage, ArpPage, FxPage, MasterPage
  Widgets/                   reusable UI: VaporWidgets, Meters, EqCurve, WavetableDisplay
docs/
  RealtimeSafety.md          rules for anything that runs on the audio thread
CMakeLists.txt
.github/workflows/build.yml  Win/mac/Linux build + pluginval validation + tag release
```

## Contributing

The project is in active development; external contributions aren't on a
defined schedule yet. Bug reports and feedback are very welcome — please open
an issue. If you submit a patch that touches the audio thread,
[docs/RealtimeSafety.md](docs/RealtimeSafety.md) is the rulebook.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

MIT — see [LICENSE](LICENSE).
