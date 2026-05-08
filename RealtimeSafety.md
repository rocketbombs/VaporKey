# Realtime safety

VaporKey runs `processBlock` on the host's audio thread. That thread has a
hard real-time deadline every audio buffer (typically 1-20 ms): any operation
that can block, take a lock the message thread might hold, or call into the
heap allocator can stall the callback and cause an audible glitch — or, in
the worst case (multi-instance hosts under contention), freeze the host
entirely.

This document captures what is and isn't allowed inside `processBlock` and
the helpers it calls (everything reachable from `Arpeggiator::process`,
`SynthEngine::renderBlock`, `FxChain::process`, `WTVoice::renderNextBlock`,
and `VaporKeyAudioProcessor::filterMidi` / `updateMacroSums`).

## Rules

1. **No heap allocation.** Don't construct anything that may allocate
   (`std::vector::push_back`, `juce::Array::add` past reserved capacity,
   `std::make_shared`, `juce::String` concatenation, `juce::dsp::IIR::
   Coefficients::make*`, anything that returns a `ReferenceCountedObjectPtr`).
2. **No locks the message thread can hold.** Never take a mutex, condition
   variable, file lock, or `juce::AudioProcessor::getCallbackLock()` (that
   one is *for* the message thread to grab so it can pause us).
3. **No system calls.** No file I/O, no logging, no window calls, no DAW
   transport queries beyond `getPlayHead()->getPosition()` (which is
   designed for this).
4. **No throwing.** Audio code is `noexcept` in spirit even if not declared.

## How we enforce these

### Pre-reserved scratch buffers

Anything that needs a temporary `MidiBuffer`, `Array`, or `vector` reserves
its storage in `prepareToPlay` (or the subsystem's `prepare`). The audio
path then uses `clear()` / `clearQuick()` / `MidiBuffer::clear()` between
blocks, which keeps the allocation but resets the size.

Examples:
- `Arpeggiator::prepare` reserves `passBuf`, `noteEventsBuf`,
  `noteSamplesBuf`, `activeBuf`, `orderedBuf`, `held`, `latched`.
- `VaporKeyAudioProcessor::prepareToPlay` reserves `monoFilterBuf` and
  `monoHeldNotes`.

When you add a new audio-thread `Array` or `MidiBuffer`, add a matching
`ensureStorageAllocated` / `ensureSize` call to the owning subsystem's
`prepare`.

### IIR coefficient cache

`juce::dsp::IIR::Coefficients::makeLowShelf` (and all `make*` siblings)
construct a `ReferenceCountedObject` — a heap allocation per call. That's
fine on the message thread but lethal in `processBlock`.

`FxChain` caches the last `(gain, freq)` inputs to each EQ band and only
calls `make*` when the inputs change. The same pattern applies if you add
another IIR filter: store the previous parameter values, compare with an
epsilon, only rebuild on change.

### Cross-thread parameter access

The audio thread reads parameters via the `SynthParams` cache: pointers
to `std::atomic<float>` published once by `Parameters::cache` and never
modified afterwards. Loads use `->load()` (or `*ptr` since these are
`std::atomic<float>*`). Don't look up parameters by ID inside
`processBlock` — `apvts.getRawParameterValue("...")` does a string lookup.

Live MIDI state (pitch bend, mod wheel, aftertouch) is also stored in
`SynthParams` as `std::atomic<float>` and written by `filterMidi` on the
audio thread, then read by voices in the same block.

### Custom wavetables (atomic shared_ptr swap)

Drag-and-drop / file-chooser loads a new wavetable on the message thread,
then publishes it with `std::atomic_store(&synthParams.customTables[i], ...)`.
Voices read the table per block with `std::atomic_load(&...)`. The old
wavetable's destructor runs whenever the last reference drops — which may
be the audio thread on a subsequent swap. That's fine because `Wavetable`
itself is just an array of floats; freeing it is a single `delete` (still
a heap operation, but bounded and infrequent — one per swap, not per block).

### Visualisation handoff

`VisData` is read by editor `juce::Timer`s (~60 Hz). The audio thread writes
per-block scope samples + peak / RMS into atomic fields with relaxed memory
ordering — the editor seeing slightly stale values is fine. The scope ring
buffer index uses release-store / acquire-load to keep tearing invisible.

### RNG seeding

Every random source seeds itself randomly in its constructor:
`WTVoice::WTVoice` calls `rng.setSeedRandomly()`, `Arpeggiator::Arpeggiator`
does the same. Without this, multiple plugin instances would all start with
JUCE's default seed and produce identical "noise" / "grit" / "drift" /
random-arp streams — which sums coherently and reads as aliasing rather
than noise. If you add a new random source on the audio path, seed it the
same way.

## Where to put new audio code

| Concern                              | File                       |
|--------------------------------------|----------------------------|
| Per-voice synthesis                  | `Source/Voice.{h,cpp}`     |
| Voice orchestration / pool           | `Source/SynthEngine.{h,cpp}`|
| MIDI -> note scheduling              | `Source/Arpeggiator.{h,cpp}`|
| Post-synth FX                        | `Source/FxChain.{h,cpp}`   |
| Wavetable building (offline)         | `Source/Wavetable.cpp`     |
| Wavetable file import (offline)      | `Source/WavetableImport.{h,cpp}` |
| APVTS layout / param IDs / enums     | `Source/Parameters.{h,cpp}`|
| Factory / user preset I/O            | `Source/PresetStore.{h,cpp}`|
| Top-level audio orchestration        | `Source/PluginProcessor.{h,cpp}` |

If a piece of code is only invoked from the message thread (preset I/O,
wavetable import, GUI), normal C++ rules apply — allocate freely, log,
take locks. If it lives anywhere reachable from `processBlock`, the rules
above apply.
