# Changelog

All notable changes to VaporKey are documented here. The project follows
[Semantic Versioning](https://semver.org/) loosely while pre-1.0: minor
versions may break parameter or preset compatibility.

## [Unreleased]

### Fixed
- Multi-instance host freezes (FL Studio and others) caused by audio-thread
  heap allocations:
  - `processArpeggiator` and `filterMidi` now reuse pre-sized scratch buffers
    instead of allocating a `MidiBuffer` and `juce::Array`s every block.
  - The 3-band EQ caches the last (`gain`, `freq`) inputs and only rebuilds
    the IIR coefficients on change — the JUCE `Coefficients::make*` helpers
    each allocate a `ReferenceCountedObject`, which is fatal on the audio
    thread under multi-instance contention.
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
