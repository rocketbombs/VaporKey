#pragma once
#include "Wavetable.h"

// Per-shape time-domain frame generators for WavetableLibrary. These live in
// their own translation unit because keeping all 23 of them alongside the
// Wavetable class implementation overflows MSVC's PDB type server during
// Release compilation (CL.exe crashes inside CloseTypeServerPDB on the
// Windows CI runner). Splitting reduces the per-file type-info load and
// keeps each generator independently optimisable.
namespace WavetableShapes
{
    using Buf = std::array<float, Wavetable::kFrameSize>;

    void buildBasic    (int f, Buf& b);
    void buildSaws     (int f, Buf& b);
    void buildSquares  (int f, Buf& b);
    void buildVocal    (int f, Buf& b);
    void buildBell     (int f, Buf& b);
    void buildDigital  (int f, Buf& b);
    void buildHarmonic (int f, Buf& b);
    void buildGlass    (int f, Buf& b);
    void buildReso     (int f, Buf& b);
    void buildSync     (int f, Buf& b);
    void buildRingMod  (int f, Buf& b);
    void buildWavefold (int f, Buf& b);
    void buildVowels   (int f, Buf& b);
    void buildChoir    (int f, Buf& b);
    void buildWhisper  (int f, Buf& b);
    void buildOrgan    (int f, Buf& b);
    void buildPluck    (int f, Buf& b);
    void buildSawteeth (int f, Buf& b);
    void buildEvenOdd  (int f, Buf& b);
    void buildTine     (int f, Buf& b);
    void buildMallet   (int f, Buf& b);
    void buildFMStack  (int f, Buf& b);
    void buildBitcrush (int f, Buf& b);
}
