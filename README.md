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
- **170 factory presets** across Bass, Lead, Pad, Pluck, Keys, Bell, FX, Arp,
  browsable from a sidebar of category buttons with a live name search
- **Realtime-safe** audio path — no allocations, locks, or I/O in `processBlock`
  (see [docs/RealtimeSafety.md](docs/RealtimeSafety.md))
- **Validated** on every push by a three-stage CI gate: a JSON preset linter
  (`VaporKeyPresetLint`), a unit + audio-regression test suite
  (`VaporKeyTests`), and [Tracktion pluginval](https://github.com/Tracktion/pluginval)
  at strictness 5

## Synth engine

**Oscillators**
- Three wavetable oscillators, mip-mapped across 10 octaves, drawing from a
  bank of **23 factory shapes** in 8-frame morphs, organised into six
  categories surfaced in a multi-column popup picker:
  - **Analog** — Basic, Saws, Squares
  - **Vocal** — Vowels, Choir, Whisper
  - **Harmonic** — Harmonic, Organ, Sawteeth, Even/Odd
  - **Inharmonic** — Bell, Glass, Tine, Mallet, Pluck
  - **Digital** — Digital, FM Stack, Bitcrush
  - **Special** — Reso, Sync, RingMod, Wavefold
- **X-MOD matrix.** Each oscillator can pick another oscillator (or none)
  as an FM source, ring modulator, or amplitude modulator, with an
  independent depth control and a small node-and-arrow diagram on the
  OSC page visualising the active routing.
- Per-osc level, pan, coarse/fine tuning, unison (1–7), detune, phase
- Drag-and-drop `.wav` import for custom wavetables
- Sub oscillator (sine / square / triangle, –1 or –2 octaves)
- Noise generator (white / pink / brown), per-channel for genuine stereo

**Filter & envelopes**
- State-variable TPT filter (LP / BP / HP) with cutoff, resonance, drive,
  key tracking, and velocity
- Antiderivative-anti-aliased (`fastTanh` ADAA) drive and post-filter
  saturation per voice
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
1. **Distortion** (Soft / Hard / Fold / Bit), 4× polyphase-IIR halfband
   oversampled so wavefolder and hard-clip harmonics don't fold back into
   the audible range
2. 3-band EQ (low shelf, parametric mid, high shelf)
3. Chorus
4. Phaser
5. Stereo ping-pong delay (free or tempo-synced)
6. **Plate reverb** — 1997 Dattorro topology (input bandwidth filter →
   diffuser cascade → modulated figure-eight tank → seven-tap stereo
   read), structurally stereo with no width control needed
7. Compressor
8. Master gain & stereo width

**MIDI**
- Configurable pitch-bend range (1–24 semitones)
- Mod wheel and aftertouch routed through the same per-block mod sum as the macros

**Presets**
- 170 factory patches authored to exercise every corner of the engine:
  every wavetable shape, every distortion type and filter mode, X-MOD FM /
  ring / AM routings, sub + noise blends, all four macros pre-wired to
  destinations, mod-wheel / aftertouch assignments, LFO sync divisions,
  pitch envelope, comp on / off, mono+legato glide, and arp variations
  across all six modes.
- Factory presets live in [`Source/Presets.json`](Source/Presets.json) and
  are baked into the binary at build time — fork the JSON to add your own.
- User presets are saved under the system app-data directory:
  `RocketBombs/VaporKey/Presets`.
- The Master page hosts a two-pane browser: a category sidebar (All / 8
  factory categories / User, with per-category counts) and a name-search
  field that filters live as you type. Prev/Next steps through the
  currently filtered view.

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

Useful CMake flags:

- `-DVAPORKEY_LTO=OFF` — disable link-time optimisation. ON by default for
  local Release builds (smaller, faster binary); CI passes OFF to keep peak
  link memory under the GitHub-hosted runners' 16 GB cap.

#### macOS (universal binary)

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release --parallel
```

#### Windows (Ninja — recommended for fast iteration)

The default CMake invocation works on Windows (it picks the Visual Studio
generator), but a Ninja-based build runs noticeably faster and matches what
CI uses. From a Developer Command Prompt for VS 2022:

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

CI uses `ilammy/msvc-dev-cmd` to set up the same MSVC environment a
Developer Prompt provides.

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
  SynthVoice.{h,cpp}         per-voice synthesis (oscs, X-MOD, sub, noise, filter, envs)
  Wavetable.{h,cpp}          mip-mapped wavetable oscillator + library scaffold
  WavetableShapes.{h,cpp}    23 shape generators (Basic..Bitcrush) + categories
  WavetableImport.{h,cpp}    drag-and-drop .wav -> custom table
  Arpeggiator.{h,cpp}        MIDI-rewriting arp, tempo-synced
  FxChain.{h,cpp}            distortion / EQ / chorus / phaser / delay / reverb / comp / width
  PlateReverb.{h,cpp}        Dattorro-topology plate reverb implementation
  PresetStore.{h,cpp}        factory + user preset save/load/rename
  Presets.{h,cpp}            JSON parser for embedded factory presets
  Presets.json               factory preset data (embedded at build)
  PresetLint.cpp             standalone JSON ↔ parameter-registry linter
  LookAndFeel.{h,cpp}        custom JUCE LookAndFeel
  Pages/                     one source pair per editor page
    OscPage, FilterEnvPage, ModPage, ArpPage, FxPage, MasterPage
    EditorLayout.h           shared layout constants for the page grid
  Widgets/                   reusable UI components
    VaporWidgets             rotary knobs, combos, toggles, common look
    Meters                   peak / RMS bar + scope ring buffer renderer
    EqCurve                  interactive 3-band EQ curve display
    WavetableDisplay         per-osc audio-reactive frame visualiser
    WavetableShapePicker     categorised multi-column popup shape picker
    OscModMatrix             X-MOD source / type / amount + routing diagram
  Tests/                     unit + audio-regression suite (VaporKeyTests target)
    TestRunner.{h,cpp}       tiny self-registering framework, single binary
    TestSupport.{h,cpp}      headless TestProcessor, audio fixtures, .wav helpers
    ParametersTests.cpp      registry / cache / defaults / range validation
    WavetableTests.cpp       factory shapes, mip selection, build-from-audio
    WavetableImportTests.cpp .wav import + retirement queue
    SynthEngineTests.cpp     mono/legato, pitch bend, mod wheel, aftertouch, voice stealing
    ArpeggiatorTests.cpp     all six modes, swing, latch, divisions, controller pass-through
    FxChainTests.cpp         distortion, EQ, delay, reverb, compressor, mono bus
    PresetTests.cpp          factory apply, state save/restore round-trip
    AudioRegressionTests.cpp end-to-end render under stress, CPU budget, click-free preset switch
docs/
  RealtimeSafety.md          rules for anything that runs on the audio thread
CMakeLists.txt
.github/workflows/build.yml  preset-lint -> tests -> Win/mac/Linux build + pluginval + tag release
```

## Testing

Both validation targets are part of the default CMake configuration:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VaporKeyPresetLint VaporKeyTests --parallel

build/VaporKeyPresetLint_artefacts/Release/VaporKeyPresetLint Source/Presets.json
build/VaporKeyTests_artefacts/Release/VaporKeyTests
# or run a subset:
build/VaporKeyTests_artefacts/Release/VaporKeyTests Wavetable
build/VaporKeyTests_artefacts/Release/VaporKeyTests --list
```

CI runs both on every push (Linux, headless via `xvfb-run`) before the
per-platform pluginval pass. Any failing test fails the build.

## Contributing

The project is in active development; external contributions aren't on a
defined schedule yet. Bug reports and feedback are very welcome — please open
an issue. If you submit a patch that touches the audio thread,
[docs/RealtimeSafety.md](docs/RealtimeSafety.md) is the rulebook.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

MIT — see [LICENSE](LICENSE).
