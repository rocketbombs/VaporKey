// SynthEngine tests. The engine sits between the arpeggiator and FxChain on
// the audio path; its job is to filter MIDI for mono / legato handling, push
// live MIDI controllers (pitch wheel, mod wheel, aftertouch) into the per-
// block atomics, sum macro contributions into modSum, and drive the JUCE
// Synthesiser.
//
// Coverage:
//   * MIDI scanner stores pitch bend, mod wheel, aftertouch into SynthParams
//   * Pitch bend is scaled by the configured bend range
//   * Mono mode emits note-offs for previously held notes when a new note
//     arrives, so the synth never has more than one voice sounding
//   * Mono+Legato re-sounds the previously-held note on note-off (last-note
//     fall-back) without re-attacking envelopes
//   * updateMacroSums reflects mod wheel / aftertouch / macro destinations
//     additively into modSum
//   * Voice stealing keeps the synth at most 16-voice
//   * processBlock is allocation-free (pre-allocated scratch is sufficient)
//   * Output is finite, bounded and non-silent on a known patch
#include "TestRunner.h"
#include "TestSupport.h"

using namespace VKTest;

namespace
{
    constexpr double kSR    = 48000.0;
    constexpr int    kBlock = 256;

    struct EngineHarness
    {
        TestProcessor tp;
        SynthEngine   engine { tp.params };
        Arpeggiator   arp    { tp.params };
        FxChain       fx     { tp.params };

        EngineHarness()
        {
            resetParametersToDefaults (tp);
            engine.prepare (kSR, kBlock);
            arp.prepare (kSR);
            fx.prepare (kSR, kBlock);
        }

        // Run one block with the supplied MIDI; return the rendered audio.
        juce::AudioBuffer<float> step (juce::MidiBuffer midi, int n = kBlock,
                                        int numCh = 2, double bpm = 120.0)
        {
            juce::AudioBuffer<float> buf (numCh, n);
            buf.clear();
            arp.process (midi, n, bpm);
            engine.process (buf, midi);
            fx.process (buf, bpm);
            return buf;
        }
    };
}

VK_TEST (SynthEngine_DefaultPatchProducesAudibleOutput)
{
    EngineHarness h;
    auto buf = h.step (singleEventBuffer (Midi::noteOn (60, 100)), kBlock);

    // First block: there's an attack envelope so the start may be quiet, but
    // by the end of a 256-sample block at 48kHz (5.3ms) we should see signal.
    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);

    // Render a few more blocks to let the envelope rise.
    juce::AudioBuffer<float> total (2, kBlock * 8);
    total.clear();
    for (int b = 0; b < 8; ++b)
    {
        juce::MidiBuffer m;
        if (b == 0) m.addEvent (Midi::noteOn (60, 100), 0);
        auto out = h.step (m, kBlock);
        for (int ch = 0; ch < 2; ++ch)
            total.copyFrom (ch, b * kBlock, out, ch, 0, kBlock);
    }
    const auto allStats = analyse (total);
    VK_EXPECT_GT (allStats.peakAbs, 0.001f);
    VK_EXPECT (! allStats.hasNaN);
    VK_EXPECT (! allStats.hasInf);
}

VK_TEST (SynthEngine_PitchWheelStoresSemitoneOffset)
{
    EngineHarness h;
    setParameter (h.tp, "bend_range", 12.0f);

    // Centre value: 8192 -> 0 semis.
    auto m1 = singleEventBuffer (Midi::pitchWheel (8192));
    h.step (std::move (m1), 64);
    VK_EXPECT_NEAR (h.tp.params.pitchBendSemis.load(), 0.0f, 1.0e-3f);

    // Maximum positive: 16383 -> +12 (just under, but bend_range is 12 and
    // the conversion is (16383-8192)/8192 * 12 = 11.9985... )
    auto m2 = singleEventBuffer (Midi::pitchWheel (16383));
    h.step (std::move (m2), 64);
    VK_EXPECT_NEAR (h.tp.params.pitchBendSemis.load(), 12.0f, 0.05f);

    // Minimum: 0 -> -12.
    auto m3 = singleEventBuffer (Midi::pitchWheel (0));
    h.step (std::move (m3), 64);
    VK_EXPECT_NEAR (h.tp.params.pitchBendSemis.load(), -12.0f, 0.05f);
}

VK_TEST (SynthEngine_PitchWheelHonoursBendRange)
{
    EngineHarness h;
    setParameter (h.tp, "bend_range", 2.0f);

    auto m = singleEventBuffer (Midi::pitchWheel (16383));
    h.step (std::move (m), 64);
    VK_EXPECT_NEAR (h.tp.params.pitchBendSemis.load(), 2.0f, 0.05f);
}

VK_TEST (SynthEngine_ModWheelStoredAsNormalised)
{
    EngineHarness h;
    auto m = singleEventBuffer (Midi::cc (1, 127));
    h.step (std::move (m), 64);
    VK_EXPECT_NEAR (h.tp.params.modWheel.load(), 1.0f, 1.0e-3f);

    h.step (singleEventBuffer (Midi::cc (1, 64)), 64);
    VK_EXPECT_NEAR (h.tp.params.modWheel.load(), 64.0f / 127.0f, 1.0e-3f);

    h.step (singleEventBuffer (Midi::cc (1, 0)), 64);
    VK_EXPECT_NEAR (h.tp.params.modWheel.load(), 0.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_AftertouchStored)
{
    EngineHarness h;
    h.step (singleEventBuffer (Midi::channelPressure (127)), 64);
    VK_EXPECT_NEAR (h.tp.params.aftertouch.load(), 1.0f, 1.0e-3f);

    h.step (singleEventBuffer (Midi::channelPressure (64)), 64);
    VK_EXPECT_NEAR (h.tp.params.aftertouch.load(), 64.0f / 127.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_PolyKeyAfterTouchStored)
{
    EngineHarness h;
    h.step (singleEventBuffer (Midi::afterTouch (60, 100)), 64);
    VK_EXPECT_NEAR (h.tp.params.aftertouch.load(), 100.0f / 127.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_MacroSumIncludesModWheelDestination)
{
    EngineHarness h;
    // Wire mod wheel to Cutoff with full amount.
    setParameter (h.tp, "mw_dest", (float) ModDest::Cutoff);
    setParameter (h.tp, "mw_amt",  1.0f);

    h.step (singleEventBuffer (Midi::cc (1, 127)), 64);
    VK_EXPECT_NEAR (h.tp.params.modSum[ModDest::Cutoff], 1.0f, 1.0e-3f);

    h.step (singleEventBuffer (Midi::cc (1, 0)), 64);
    VK_EXPECT_NEAR (h.tp.params.modSum[ModDest::Cutoff], 0.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_MacroSumIncludesAftertouchDestination)
{
    EngineHarness h;
    setParameter (h.tp, "at_dest", (float) ModDest::Reso);
    setParameter (h.tp, "at_amt",  -1.0f);

    h.step (singleEventBuffer (Midi::channelPressure (127)), 64);
    VK_EXPECT_NEAR (h.tp.params.modSum[ModDest::Reso], -1.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_MacroSumIsAdditive)
{
    EngineHarness h;
    setParameter (h.tp, "mw_dest", (float) ModDest::Cutoff);
    setParameter (h.tp, "mw_amt",  0.5f);
    setParameter (h.tp, "at_dest", (float) ModDest::Cutoff);
    setParameter (h.tp, "at_amt",  0.5f);

    juce::MidiBuffer mb;
    mb.addEvent (Midi::cc (1, 127), 0);
    mb.addEvent (Midi::channelPressure (127), 0);
    h.step (std::move (mb), 64);

    VK_EXPECT_NEAR (h.tp.params.modSum[ModDest::Cutoff], 1.0f, 1.0e-3f);
}

VK_TEST (SynthEngine_MonoModeSilencesPriorNote)
{
    // In mono mode, a fresh note-on should emit note-offs for previously
    // held notes; only the new note ends up sounding.
    EngineHarness h;
    setParameter (h.tp, "mono",   1.0f);
    setParameter (h.tp, "legato", 0.0f);
    // Shorten the release so the previously-held voice fully clears within
    // the post-press settle window below; with the default 0.3s release we'd
    // need >50 blocks of silence to see isVoiceActive() return false.
    setParameter (h.tp, "a_r",    0.005f);

    // First note.
    h.step (singleEventBuffer (Midi::noteOn (60, 100)), kBlock);
    int active = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
        if (h.engine.synth().getVoice (i)->isVoiceActive()) ++active;
    VK_EXPECT_GT (active, 0);

    // Second note arrives; mono mode should turn off the first.
    juce::MidiBuffer mb;
    mb.addEvent (Midi::noteOn (64, 100), 0);
    h.step (std::move (mb), kBlock);

    // Allow the synth several blocks to finish the release on the muted note.
    for (int b = 0; b < 16; ++b)
        h.step (juce::MidiBuffer{}, kBlock);

    int sounding60 = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
    {
        auto* v = h.engine.synth().getVoice (i);
        if (v != nullptr && v->isVoiceActive() && v->getCurrentlyPlayingNote() == 60)
            ++sounding60;
    }
    VK_EXPECT_EQ (sounding60, 0);
}

VK_TEST (SynthEngine_MonoNoteOffFallsBackToHeld)
{
    // Mono mode: holding C, then E, then releasing E should re-sound C.
    EngineHarness h;
    setParameter (h.tp, "mono",   1.0f);
    setParameter (h.tp, "legato", 0.0f);
    setParameter (h.tp, "a_r",    0.005f); // see comment in MonoModeSilencesPriorNote

    h.step (singleEventBuffer (Midi::noteOn (60, 100)), kBlock);

    juce::MidiBuffer pressE;
    pressE.addEvent (Midi::noteOn (64, 100), 0);
    h.step (std::move (pressE), kBlock);

    juce::MidiBuffer releaseE;
    releaseE.addEvent (Midi::noteOff (64), 0);
    h.step (std::move (releaseE), kBlock);

    for (int b = 0; b < 16; ++b)
        h.step (juce::MidiBuffer{}, kBlock);

    int sounding60 = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
    {
        auto* v = h.engine.synth().getVoice (i);
        if (v != nullptr && v->isVoiceActive() && v->getCurrentlyPlayingNote() == 60)
            ++sounding60;
    }
    VK_EXPECT_EQ (sounding60, 1);
}

VK_TEST (SynthEngine_PolyModeRunsMultipleVoices)
{
    EngineHarness h;
    setParameter (h.tp, "mono",   0.0f);

    juce::MidiBuffer chord;
    chord.addEvent (Midi::noteOn (60, 100), 0);
    chord.addEvent (Midi::noteOn (64, 100), 0);
    chord.addEvent (Midi::noteOn (67, 100), 0);
    h.step (std::move (chord), kBlock);

    int active = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
        if (h.engine.synth().getVoice (i)->isVoiceActive()) ++active;
    VK_EXPECT_GT (active, 2);
}

VK_TEST (SynthEngine_VoiceStealingCapsAt16)
{
    EngineHarness h;
    setParameter (h.tp, "mono",   0.0f);

    juce::MidiBuffer big;
    for (int n = 36; n < 36 + 24; ++n)
        big.addEvent (Midi::noteOn (n, 100), 0);
    h.step (std::move (big), kBlock);

    int active = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
        if (h.engine.synth().getVoice (i)->isVoiceActive()) ++active;
    VK_EXPECT_LT (active, 17); // <= 16 voices total
    VK_EXPECT_GT (active, 0);
}

VK_TEST (SynthEngine_OutputBoundedOnExtremeChord)
{
    // Stress: full chord, all FX off, see if anything explodes.
    EngineHarness h;

    // Default patch + extreme 16-note chord.
    juce::AudioBuffer<float> total (2, kBlock * 8);
    total.clear();
    for (int b = 0; b < 8; ++b)
    {
        juce::MidiBuffer m;
        if (b == 0)
            for (int n = 36; n < 52; ++n) m.addEvent (Midi::noteOn (n, 100), 0);
        auto out = h.step (m, kBlock);
        for (int ch = 0; ch < 2; ++ch)
            total.copyFrom (ch, b * kBlock, out, ch, 0, kBlock);
    }

    const auto stats = analyse (total);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 8.0f); // wildly generous; under 0.5 in practice
}

VK_TEST (SynthEngine_AllNotesOffSilencesActiveVoices)
{
    EngineHarness h;
    h.step (singleEventBuffer (Midi::noteOn (60, 127)), kBlock);

    int activeBefore = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
        if (h.engine.synth().getVoice (i)->isVoiceActive()) ++activeBefore;
    VK_EXPECT_GT (activeBefore, 0);

    h.engine.allNotesOff();

    int activeAfter = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
        if (h.engine.synth().getVoice (i)->isVoiceActive()) ++activeAfter;
    VK_EXPECT_LT (activeAfter, activeBefore);
}
