# Changelog

All notable changes to VaporKey are documented here. The project follows
[Semantic Versioning](https://semver.org/) loosely while pre-1.0: minor
versions may break parameter or preset compatibility.

## [Unreleased]

### Added
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
