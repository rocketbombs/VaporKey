# Changelog

All notable changes to VaporKey are documented here. The project follows
[Semantic Versioning](https://semver.org/) loosely while pre-1.0: minor
versions may break parameter or preset compatibility.

## [Unreleased]

### Added
- **Factory preset bank rebuilt at 170 patches.** The 57-preset starter
  bank grew to 170 presets across Bass (21), Lead (24), Pad (24),
  Pluck (19), Keys (21), Bell (14), FX (22), and Arp (25), with every
  original preset preserved by name and content for backward
  compatibility. The new patches were authored to systematically exercise
  every corner of the engine: every wavetable shape (including the v0.5
  additions Sync / RingMod / Wavefold / Vowels / Choir / Whisper / Organ /
  Pluck / Sawteeth / Even-Odd / Tine / Mallet / FM Stack / Bitcrush),
  every X-MOD type (FM / Ring / AM) on every routing, every distortion
  type (Soft / Hard / Fold / Bit), every filter mode (LP / BP / HP) plus
  key tracking and velocity, all four macros pre-wired to mod
  destinations, mod-wheel and aftertouch assignments, LFO tempo-sync
  divisions, pitch envelope (positive *and* negative semitone offsets),
  compressor on/off, mono+legato glide, and arp variations covering all
  six modes plus latch and swing. The preset linter (`VaporKeyPresetLint`)
  validates every value in CI against the parameter registry, and the
  audio regression suite renders the full bank end-to-end.
- **Two-pane preset browser.** The Master page replaces the prior
  single-combo category filter with a vertical category sidebar
  (All / Bass / Lead / Pad / Pluck / Keys / Bell / FX / Arp / User) that
  shows the live filtered count next to each entry, and adds a name
  search field that filters the list as you type (case-insensitive
  substring match). A "Showing N / M" chip in the header keeps the size
  of the current view visible at a glance. Prev/Next, Save / Rename /
  Delete, and the now-playing label all operate over the filtered view,
  so navigating by category or text query is transparent.
- **Cross-oscillator modulation (X-MOD).** Each oscillator can now use any
  other oscillator (or none) as an FM source, ring modulator, or amplitude
  modulator. The OSC page hosts an X-MOD matrix below the per-osc strips
  with a source combo, mod-type combo, and amount slider per destination,
  plus a small node-and-arrow diagram visualising the active routing. FM
  uses pre-pan modulator output so depth is independent of the source's
  level/pan settings; ring and AM operate on the post-mix signal.
  Parameter ids: `oscN_mod_src`, `oscN_mod_type`, `oscN_mod_amt`.
- **14 new factory wavetables.** The Basic / Saws / Squares / Vocal / Bell /
  Digital / Harmonic / Glass / Reso bank grew to 23 shapes:
  *Sync* (hard-sync sweep), *RingMod* (sine-product), *Wavefold* (symmetric
  triangle wavefolder), *Vowels* (A/E/I/O/U formant morph using Peterson &
  Barney 1952 formant tables), *Choir* (mixed-formant ensemble), *Whisper*
  (high-formant breathy), *Organ* (Hammond-style 9-drawbar additive),
  *Pluck* (Karplus-Strong-flavoured exponentially-damped harmonics),
  *Sawteeth* (odd-harmonic saw with rolloff), *Even/Odd* (even-vs-odd
  balance morph), *Tine* (Rhodes-style inharmonic clang), *Mallet*
  (vibraphone-mode partials), *FM Stack* (3-operator stacked FM),
  *Bitcrush* (sweepable-bit-depth quantised sine). The legacy 0..9 indices
  are frozen, so every existing preset still loads with the same timbre.
- **Categorised wavetable selector.** The OSC page's shape combo is now a
  custom multi-column popup grouped by Analog / Vocal / Harmonic /
  Inharmonic / Digital / Special, so the 23-shape bank fits in one
  scroll-free view with related timbres sitting next to each other. The
  selector also surfaces the active category as a small caption inside
  the picker so the current sound's family is visible without opening
  the menu.
- **Dattorro plate reverb.** Replaces JUCE's FreeVerb with a 1997 Dattorro
  topology (input bandwidth filter → 4-stage diffuser cascade → figure-eight
  tank with cross-coupled half-paths, modulated all-passes for flutter-echo
  break-up, damping low-pass, and seven-tap stereo output read from the
  tank's interior). Denser, smoother tail; structural stereo de-correlation
  with no width control needed. The `reverb_size` and `reverb_damp`
  parameters now drive the tank's decay coefficient and damping LP cutoff
  respectively - same parameter ids and ranges, so existing presets keep
  working.
- **4× oversampled distortion.** The FX chain's distortion stage (soft / hard
  / fold / bit) now runs through a 4× polyphase-IIR halfband oversampler
  (zero-latency, sub-sample group delay). Hard-clip and wavefolder generate
  broadband harmonics that previously aliased hard back into the audible
  range; oversampling pushes the alias band beyond Nyquist before the
  decimation filter pulls it back down. CPU cost is incurred only when
  distortion is engaged.
- **Antiderivative anti-aliased per-voice saturation.** The pre-filter drive
  and post-filter analog saturation in `WTVoice` now use first-order ADAA
  on `fastTanh`. Per-sample cost is one log + a handful of FLOPs - cheap
  enough to keep all 16 voices in budget without resorting to per-voice
  oversampling. The technique replaces `f(x[n])` with the average of `f`
  over the input interval, which is exactly the bandlimited continuous-time
  output for a piecewise-linear input reconstruction.
- **Validation suite (`VaporKeyTests`).** New CMake target builds a single
  console runner that exercises the synth engine, arpeggiator, FX chain,
  wavetable mip-mapping, .wav import, parameter registry, factory presets,
  state save/restore, and end-to-end audio (CPU budget + click-free preset
  switch). Tests cover MIDI edge cases (mono/legato fall-back, pitch-bend
  range, mod wheel, aftertouch, voice stealing) and adversarial signal flow
  (extreme drives + feedback paths checked for NaN/Inf/denormals). CI runs
  the full suite on Linux (`xvfb-run`) between the preset linter and the
  per-platform pluginval pass.

### Fixed
- **Arp toggle no longer leaves a stuck note droning under the sequence.**
  When the arp turned ON while a key was already held (its note-on having
  passed through directly to the synth in OFF mode), the synth voice for
  that key kept sustaining while the arp arpeggiated on top, because the
  arp had no record of which notes pass-through had already started. The
  arp now tracks the held set in *both* modes and, on the OFF -> ON
  transition, emits matching note-offs so the synth releases those voices
  before the arp takes over. Symmetrically, on the ON -> OFF transition
  the arp re-emits note-ons for everything still physically held so the
  synth picks those keys back up as direct voices (the arp had been
  suppressing pass-through during the session). Latch is seeded from the
  held set on OFF -> ON so a user who enables latch+arp while holding
  keys gets the held chord latched, the same way it would behave if the
  keys had been pressed after the arp turned on.
- **Mono + legato now actually glides.** The previous flow set a "skip
  envelope retrigger" flag on the held voice and emitted a noteOff/noteOn
  pair, expecting the synth to land the new note on the same voice. JUCE's
  `findFreeVoice` doesn't pick voices that are still in release, so the
  noteOn went to a fresh idle voice instead — the held voice was left
  decaying with its flag never consumed, and the new voice attacked from
  zero. Net effect: every "legato" press behaved like a retrigger, *and*
  the synth briefly used two voices per legato note (released + attacking).
  Fixed by arming a hand-off flag on the active voice; its `stopNote` (when
  the synth processes the noteOff) now clears `currentlyPlayingNote`
  without releasing envelopes, so `findFreeVoice` picks that same voice up
  for the immediately-following noteOn and the new `startNote` glides on
  preserved envelope / phase / filter state.
- Mono bus output: voice render and FX chain previously aliased the right
  channel pointer to the left buffer, then ran every per-channel operation
  (distortion, EQ, pan summation) twice into the same memory, doubling gain
  and scrambling cross-channel routing (ping-pong delay, M/S width). Voices
  now sum L+R to a single mono fold; the FX chain runs on a stereo scratch
  in mono mode and mixes back down at the end.
- Preset switching now holds the audio callback lock across silence, FX
  reset, *and* the APVTS state swap. Previously the lock was released
  between silence and apply, briefly exposing a "voices muted but
  parameters still old" intermediate state.

### Changed
- Custom wavetable lifetime is now managed by an explicit
  `WavetableRetirementQueue`. Replaced `shared_ptr` instances are handed
  to a message-thread-owned queue that drops them once their refcount
  shows the audio thread has moved on, so the heap deallocation never
  runs on the audio path.
- Wavetable shape generators moved to their own translation unit
  (`WavetableShapes.cpp`). The 23-shape bank lives there as 23 free
  functions; `Wavetable.cpp` only owns the mip-map oscillator and the
  library scaffold. The split was forced by an MSVC ICE in
  `WavetableLibrary`'s old combined-TU constructor and turns out to be
  the right factoring on its own merits.

### Build / CI
- **Windows builds switched to Ninja.** A multi-PR investigation
  (#34–#37) traced a recurring `cl : command line error D8040: error
  creating or communicating with child process` to MSBuild's CL task
  layer: JUCE's `juce_recommended_config_flags` adds `/MP` (parallel
  workers per cl invocation) which CMake's Visual Studio generator
  translates into `<MultiProcessorCompilation>true</MultiProcessorCompilation>`,
  on top of the project-level concurrency `cmake --parallel` already
  drives. On the 4-core windows-2022 runner this stacked into ~8 cl.exe
  workers each peaking ~2 GB RSS on JUCE-template-heavy TUs and one
  child OOM-killed mid-compile. Stripping `/MP` from JUCE's interface
  options exposed a separate "all sources passed to one cl.exe" memory
  bloat; `UseMultiToolTask=true` exposed a third parallelism layer that
  ignored `cmake --parallel`. Switching the generator to Ninja sidesteps
  the entire MSBuild + CL task stack: each `.cpp` gets its own short-lived
  `cl.exe`, `cmake --build --parallel` directly governs how many are alive,
  no `/MP`, no batching, no MultiToolTask pool.
- **`cl` switched to `/Z7` debug info.** Embeds debug records in the
  `.obj` instead of routing through `mspdbsrv.exe`'s PDB type server,
  which had its own class of parallel-build flakiness. Linker strips the
  embedded records when `/DEBUG` isn't passed, so shipped Release
  binaries are unaffected.
- **`/MP` stripped from JUCE's `juce_recommended_config_flags`** at the
  CMake level. Helpful for any local developer who keeps using the
  Visual Studio generator; Ninja-based builds ignore it.
- **`/Od` on `WavetableShapes.cpp` for MSVC.** The Ninja switch surfaced
  the previously-masked underlying issue: cl.exe's UTC backend hits
  `fatal error C1001: Internal compiler error` (`p2/main.cpp:258`) when
  optimising the 23-shape generator pack under `/Ox`. Per-file `/Od`
  works around the ICE; the shape builders run once at synth init to fill
  36 frames × 4096 samples each, so the optimisation delta is invisible
  at runtime.
- **Windows Defender exclusions** for `$GITHUB_WORKSPACE`, the MSVC and
  Windows Kits install dirs, and the build processes themselves
  (`cl.exe`, `link.exe`, `cmake.exe`, `ninja.exe`). Defender can briefly
  hold the `.obj` files cl is writing, which used to surface as
  intermittent compiler-driver errors.
- Cross-platform parallelism caps: Linux and Windows both build with
  `--parallel 2` and `-DVAPORKEY_LTO=OFF` in CI to keep peak RSS under
  the GitHub-hosted runners' 16 GB cap (LTO-linking JUCE +
  `juce_dsp::Oversampling` + the plate reverb instantiations alone was
  enough to push us over).

## [0.4.1] - 2026-05-08

### Added
- Mod wheel and channel aftertouch are now real modulation sources. Each
  has a destination + amount on the Mod page (next to the four macros)
  and feeds the same per-block mod-sum the macros do, so any existing
  destination (cutoff, position, FX mix, …) reacts to live MIDI.
- CI: every build is now validated with Tracktion's pluginval at
  strictness 5 (Linux uses xvfb-run for the headless editor pass).
- CI: tag pushes (`vX.Y.Z`) draft a GitHub Release with per-platform
  bundle zips, ready for the maintainer to review and publish.

### Fixed
- Multi-instance host freezes (FL Studio and others) caused by audio-thread
  heap allocations:
  - `processArpeggiator` and `filterMidi` now reuse pre-sized scratch buffers
    instead of allocating a `MidiBuffer` and `juce::Array`s every block.
  - The 3-band EQ caches the last (`gain`, `freq`) inputs and only rebuilds
    the IIR coefficients on change — the JUCE `Coefficients::make*` helpers
    each allocate a `ReferenceCountedObject`, which is fatal on the audio
    thread under multi-instance contention.
  - `arpHeld` and `arpLatched` are now pre-reserved in `prepareToPlay`,
    closing the last `juce::Array::add` path that could allocate on the
    audio thread under fat chords.
- Mono+Legato no longer "skips the attack" on the first poly note after
  switching back to poly. `markVoicesLegato` only flags voices that are
  currently active, so the skip-retrigger flag can't be left stale on
  the 15 idle voices.
- Noise generation that summed coherently into aliasing-like buzz:
  - Each `WTVoice` now seeds its `juce::Random` randomly at construction,
    so 16 voices in a chord no longer share an identical noise stream.
  - The arpeggiator's RNG also seeds randomly, so multiple instances no
    longer step in lockstep.
  - Pink and brown noise now use independent left/right chains for genuine
    stereo decorrelation, replacing the prior mono-pink + raw-white mix
    that injected high-frequency hiss only on the right channel.
  - Brown noise switched from a hard-clipped random walk to a leaky
    integrator — no more DC drift or rail clicks.
  - "Vibe" hiss is per-channel instead of per-voice mono, so it stops
    summing coherently across polyphony.
- `setStateInformation` now silences voices and clears FX tails before
  swapping the state tree, preventing clicks on session reload.
- Mono **Legato** mode: previously the parameter existed but had no effect;
  it now glides into the new note without re-attacking the envelopes when
  transitioning while another note is held.

### Performance
- Hoisted the equal-power pan trig and `1/√unison` out of the per-sample
  unison inner loop into a per-block precompute — measurable saving on
  the synth's hottest path.

### Removed / Cleaned
- Dead code: `WTVoice::retargetNote`, `WTVoice::noteHeld`, and
  `arpUpDir` (none were referenced).

## [0.4.0] - 2026-05-08

The first version with the audio-reactive UI overhaul, JSON-driven preset
library, drag-and-drop wavetable import, arpeggiator, tempo-synced LFOs,
user preset save/load/rename, and the Grit/Vibe/Drift/Sat warmth controls.

> Earlier development versions did not have a stable changelog.
