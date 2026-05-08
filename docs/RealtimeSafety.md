# Realtime Safety

VaporKey's audio thread runs in `processBlock` and the JUCE `Synthesiser`
voices it drives. Anything called from there - directly or transitively - has
to be allocation-free, lock-free, and bounded in time. This file is the
checklist; if you change a hot-path file (`PluginProcessor.cpp`,
`SynthEngine.cpp`, `Arpeggiator.cpp`, `FxChain.cpp`, `SynthVoice.cpp`) check
it again.

## Thread roles

- **Audio thread**: `processBlock` and everything it calls. Owns
  `SynthParams::modSum`, the arpeggiator state, all FX state, and per-voice
  state.
- **Message thread**: UI, parameter changes from the host, drag-and-drop /
  file-chooser, preset save/load, state save/restore.
- **Host parameter thread**: writes APVTS atomics; the audio thread reads
  them via the cached `std::atomic<float>*` pointers in `SynthParams`.

## Hot-path rules

1. **No heap allocation.** No `new`, no `std::vector::push_back` past reserved
   capacity, no `juce::String` concatenation, no `juce::Array::add` past
   reserved capacity. Reserve scratch buffers in `prepare*` and reuse them
   (see `Arpeggiator::prepare`, `SynthEngine::prepare`).
2. **No locks.** No `juce::CriticalSection`, no `std::mutex`. The one
   exception is `juce::ScopedLock(getCallbackLock())` taken on the **message
   thread** during preset switch - it blocks the message thread waiting for
   the audio thread, never the other way around.
3. **No file or network I/O.** Disk reads happen on the message thread
   (preset load, custom wavetable import); the result is published to the
   audio thread via atomic shared_ptr swap (`std::atomic_store` on
   `SynthParams::customTables[i]`).
4. **No JUCE coefficient builders inside the loop.**
   `juce::dsp::IIR::Coefficients::makeXxx` allocates a
   `ReferenceCountedObject` every call - lethal in a per-sample loop. Cache
   the input parameters and rebuild only when they change (see the EQ path
   in `FxChain::process`).
5. **Bounded work per block.** The cost of `processBlock` must scale with
   block size, not with parameter values. No "while voice is decaying" style
   loops without a fixed bound.
6. **No `printf`/`DBG` in shipping code.** Stdio takes locks.

## Cross-thread state and atomics

- **APVTS parameters** are read via the cached atomic pointers in
  `SynthParams` (filled by `Parameters::cache`). `std::atomic<float>::load()`
  with the default `memory_order_seq_cst` is fine here - there's no
  dependent state.
- **Live MIDI state** (`pitchBendSemis`, `modWheel`, `aftertouch`, `bpm`)
  lives in `SynthParams` as `std::atomic`. The audio thread writes them
  inside `SynthEngine::filterMidi` and reads them in voices and
  `SynthEngine::updateMacroSums`.
- **Macro sum table** (`SynthParams::modSum`) is a plain `float[]` rewritten
  fresh every block by `SynthEngine::updateMacroSums` and read by voices and
  `FxChain` in the same block. No cross-thread access; no atomic needed.
- **Custom wavetables** (`SynthParams::customTables[i]`) is a
  `std::shared_ptr<Wavetable>`. The message thread publishes a new table via
  `std::atomic_exchange`; voices observe the swap with `std::atomic_load`.
  The previous shared_ptr returned by the exchange is handed to a
  `WavetableRetirementQueue` owned by the processor, which keeps a reference
  until a periodic message-thread sweep finds `use_count() == 1` (the queue
  is the sole holder, meaning every voice that snapshotted the old table
  has finished its block) and drops it. This guarantees the heap
  deallocation always runs on the message thread - relying on `shared_ptr`
  refcount traffic alone could otherwise leave the audio thread as the last
  holder when a voice's local snapshot in `renderNextBlock` destructs, and
  the resulting `~Wavetable` (hundreds of KB of mip data) would deallocate
  on the audio path.
- **VisData** (peak / RMS / scope ring buffer) is single-writer (audio
  thread, end of `processBlock`), many-reader (UI timers). Plain `float`
  ring + `std::atomic<uint32_t>` write index with `release` ordering. UI
  reads with `acquire`; occasional tearing of one sample at 60 Hz repaint is
  invisible.

## Block-order invariants

`processBlock` runs in this order; do not reorder without re-checking
modulation correctness:

1. **Tempo update** - `getPlayHead()` query, then `synthParams.bpm.store`.
2. **Arpeggiator** - transforms `midi` (note events become a stepped
   sequence; CCs and pitch bend pass through).
3. **Engine** - `filterMidi` (mono/legato + stores CC1/aftertouch into
   `SynthParams` atomics), then `updateMacroSums` (reads those atomics
   into `modSum`), then `synthesiser.renderNextBlock` (voices read
   `modSum`).
4. **FX chain** - reads `modSum` (e.g. for Width), processes audio.
5. **Visualization snapshot** - peak, RMS, scope ring buffer.

> Macros must be summed *after* the MIDI scan: a CC1 or aftertouch event
> that arrives in the current buffer needs to land in `modSum` before voices
> render. Summing first leaves a one-callback lag and starts same-block
> note-ons with stale modulation.

## Preset switching

Preset apply (`loadFactoryPreset`, `loadUserPresetByName`,
`setStateInformation`) happens on the **message thread** through the
`applyPresetUnderLock` helper:

1. Take `getCallbackLock()` so `processBlock` cannot run.
2. Call `engine.allNotesOff()` to flush voices, and `fx.reset()` to clear
   delay / reverb / chorus / EQ state. Without this the old envelope tails
   and FX feedback ride the new patch's gain and produce loud bursts.
3. Apply the new APVTS state (factory preset, user preset XML, or full
   host state tree) inside the same locked scope.
4. Release the lock; the next callback runs with the clean state.

Holding the lock across **all** of steps 2 and 3 matters: releasing between
them would let a callback run with voices muted and FX cleared but the old
parameter values still wired up, briefly producing audible artefacts from
the half-applied transition.

Custom wavetable loads (`loadCustomWavetable`, `clearCustomWavetable`) are
not part of this locked sequence - they do file I/O and we do not want to
block the audio thread waiting on disk. They are RT-safe on their own:
`std::atomic_exchange` publishes the new shared_ptr and the previous one is
handed to the `WavetableRetirementQueue` so the audio thread is never the
last holder.

The lock is fine here because the audio thread is *the lock holder we're
waiting for*, not the other way around: the message thread blocks until the
current callback returns, then runs to completion before the next callback
starts.

## When in doubt

If a change adds something new on the audio path, ask:

- Does this allocate? (`new`, container growth, JUCE refcounted helpers, `juce::String`...)
- Does this take a lock?
- Does this do I/O?
- Could a reordering (compiler or CPU) make a reader see partial state?

A "no" to all four means it's hot-path safe.
