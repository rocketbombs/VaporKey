# VaporKey

A CPU-efficient wavetable synth VST3 with vaporwave/synthwave aesthetics and analog-style warmth controls. Targeted at FL Studio and other VST3 hosts.

## Features

- **3 wavetable oscillators** with morphing position, 6 factory shape banks (Basic, Saws, Squares, Vocal, Bell, Digital), per-osc unison (1-7 voices) with detune & spread, coarse/fine tuning, level & pan.
- **Band-limited** wavetables (mip-mapped per octave) — clean across the keyboard.
- **State-variable filter** (LP / BP / HP) with mod env and LFO routing.
- **Two ADSR envelopes** (amp + assignable mod), two **LFOs** (cutoff & wavetable position).
- **"Analog" warmth section**:
  - **Grit** — random per-sample phase jitter
  - **Vibe** — subtle background hiss
  - **Drift** — slow per-osc pitch drift (decorrelated)
  - **Sat** — soft tanh saturation
- **FX**: Chorus, stereo ping-pong delay, plate-style reverb.
- **16-voice polyphony**, lock-free parameter access.
- **Vaporwave UI**: neon magenta/cyan palette, perspective grid, scan-lined sun, glowing knobs.

## Build (local)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VaporKey_VST3 --parallel
```

The compiled VST3 will be in `build/VaporKey_artefacts/Release/VST3/VaporKey.vst3`.

## Install in FL Studio (Windows)

1. Grab `VaporKey.vst3` from the latest GitHub Actions run (artifact: `VaporKey-Windows-VST3`).
2. Drop it into `C:\Program Files\Common Files\VST3\` (or any folder FL Studio scans).
3. In FL Studio: `Options → Manage plugins → Find more plugins`. VaporKey appears under *Generators*.

## License

MIT (see source headers).
