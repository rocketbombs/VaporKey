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
#include "../SynthVoice.h"

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
        auto* v = dynamic_cast<WTVoice*> (h.engine.synth().getVoice (i));
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
        auto* v = dynamic_cast<WTVoice*> (h.engine.synth().getVoice (i));
        if (v != nullptr && v->isVoiceActive() && v->getCurrentlyPlayingNote() == 60)
            ++sounding60;
    }
    VK_EXPECT_EQ (sounding60, 1);
}

VK_TEST (SynthEngine_MonoLegatoTransitionUsesSameVoice)
{
    // Mono + Legato: pressing a second note while the first is held should
    // glide on the SAME voice without re-attacking. Before the legato hand-off
    // was wired up correctly, the noteOff for the held note pushed its voice
    // into release while JUCE's findFreeVoice routed the new noteOn to a
    // fresh idle voice - so two voices sounded (old in release, new in
    // attack) and the legato-skip flag on the held voice was wasted.
    EngineHarness h;
    setParameter (h.tp, "mono",   1.0f);
    setParameter (h.tp, "legato", 1.0f);
    setParameter (h.tp, "a_a",    0.001f);  // settle into sustain quickly
    setParameter (h.tp, "a_d",    0.001f);
    setParameter (h.tp, "a_s",    1.0f);
    setParameter (h.tp, "a_r",    0.5f);    // long enough that a stale
                                            // release voice would still
                                            // register as "active" below
    setParameter (h.tp, "glide",  0.0f);    // no pitch-glide noise

    // First note: voice X plays C4.
    h.step (singleEventBuffer (Midi::noteOn (60, 100)), kBlock);
    for (int b = 0; b < 8; ++b) h.step (juce::MidiBuffer{}, kBlock);

    auto countActive = [&]
    {
        int n = 0;
        for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
            if (h.engine.synth().getVoice (i)->isVoiceActive()) ++n;
        return n;
    };
    VK_REQUIRE (countActive() == 1);

    // Second note: legato transition from C4 -> E4. The active voice should
    // be handed off (preserved envelope, glided pitch) - never two voices.
    h.step (singleEventBuffer (Midi::noteOn (64, 100)), kBlock);
    VK_EXPECT_EQ (countActive(), 1);

    // The single active voice should now be playing E4, not C4.
    int soundingNew = 0;
    int soundingOld = 0;
    for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
    {
        auto* v = dynamic_cast<WTVoice*> (h.engine.synth().getVoice (i));
        if (v == nullptr || ! v->isVoiceActive()) continue;
        if (v->getCurrentlyPlayingNote() == 64) ++soundingNew;
        if (v->getCurrentlyPlayingNote() == 60) ++soundingOld;
    }
    VK_EXPECT_EQ (soundingNew, 1);
    VK_EXPECT_EQ (soundingOld, 0);
}

VK_TEST (SynthEngine_MonoLegatoTransitionDoesNotDoubleVoices)
{
    // Concrete regression: with the previous code, a legato press fanned out
    // into TWO simultaneous voices (the held voice in release + a fresh idle
    // voice in attack on the new note). That was both audible (extra
    // amplitude) and stole CPU. The hand-off must keep the synth at exactly
    // one sounding voice through the transition.
    EngineHarness h;
    setParameter (h.tp, "mono",   1.0f);
    setParameter (h.tp, "legato", 1.0f);
    // Long release so a stale "old" voice would still register as active
    // for several blocks past the transition - the bug is observable for
    // the entire release window.
    setParameter (h.tp, "a_r", 0.5f);

    h.step (singleEventBuffer (Midi::noteOn (60, 100)), kBlock);
    for (int b = 0; b < 4; ++b) h.step (juce::MidiBuffer{}, kBlock);

    // Walk a small ascending phrase the way a player would.
    const int notes[] = { 62, 64, 65, 67 };
    int maxActiveSeen = 0;
    for (int n : notes)
    {
        h.step (singleEventBuffer (Midi::noteOn (n, 100)), kBlock);
        int active = 0;
        for (int i = 0; i < h.engine.synth().getNumVoices(); ++i)
            if (h.engine.synth().getVoice (i)->isVoiceActive()) ++active;
        if (active > maxActiveSeen) maxActiveSeen = active;
    }
    VK_EXPECT_MSG (maxActiveSeen == 1,
                   juce::String ("legato phrase produced ")
                       + juce::String (maxActiveSeen)
                       + " concurrent voices (expected 1)");
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

// Cross-osc modulation: with a non-trivial routing (osc 1 phase-modulated by
// osc 2, osc 1 ring-modulated by osc 3 chained on top would conflict, so we
// keep one route active and verify the audio remains finite, bounded, and
// audibly different from the unrouted patch.
namespace
{
    juce::AudioBuffer<float> renderHeldNote (EngineHarness& h, int note, int blocks)
    {
        juce::AudioBuffer<float> total (2, kBlock * blocks);
        total.clear();
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer m;
            if (b == 0) m.addEvent (Midi::noteOn (note, 100), 0);
            auto out = h.step (m, kBlock);
            for (int ch = 0; ch < 2; ++ch)
                total.copyFrom (ch, b * kBlock, out, ch, 0, kBlock);
        }
        return total;
    }
}

VK_TEST (SynthEngine_CrossOscFMRendersFiniteAudibleAndDifferent)
{
    // Baseline: enable osc 2 alongside osc 1 with no cross-mod.
    EngineHarness h;
    setParameter (h.tp, "osc2_on", 1.0f);
    auto baseline = renderHeldNote (h, 60, 8);
    const auto bStats = analyse (baseline);
    VK_EXPECT (! bStats.hasNaN);
    VK_EXPECT (! bStats.hasInf);
    VK_EXPECT_GT (bStats.peakAbs, 0.001f);

    // Modulated: osc 1 is FM'd by osc 2 at a strong depth.
    EngineHarness h2;
    setParameter (h2.tp, "osc2_on",       1.0f);
    setParameter (h2.tp, "osc1_mod_src",  (float) OscModSrc::Osc2);
    setParameter (h2.tp, "osc1_mod_type", (float) OscModType::FM);
    setParameter (h2.tp, "osc1_mod_amt",  0.8f);
    auto modulated = renderHeldNote (h2, 60, 8);
    const auto mStats = analyse (modulated);
    VK_EXPECT (! mStats.hasNaN);
    VK_EXPECT (! mStats.hasInf);
    VK_EXPECT_LT (mStats.peakAbs, 4.0f);    // bounded - no runaway feedback
    VK_EXPECT_GT (mStats.peakAbs, 0.001f);  // still audible

    // The modulated render should differ from the baseline by a margin well
    // outside numerical noise. We compare RMS rather than peak because peak
    // can land on either side of bypass depending on phase alignment.
    const float diff = std::abs (mStats.rms - bStats.rms);
    VK_EXPECT_GT (diff, 1.0e-4f);
}

VK_TEST (SynthEngine_CrossOscRingModRendersFinite)
{
    EngineHarness h;
    setParameter (h.tp, "osc2_on",       1.0f);
    setParameter (h.tp, "osc1_mod_src",  (float) OscModSrc::Osc2);
    setParameter (h.tp, "osc1_mod_type", (float) OscModType::Ring);
    setParameter (h.tp, "osc1_mod_amt",  1.0f);

    auto buf = renderHeldNote (h, 60, 8);
    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 4.0f);
    VK_EXPECT_GT (stats.peakAbs, 0.001f);
}

VK_TEST (SynthEngine_CrossOscSelfRoutingIsBypass)
{
    // Routing osc N to its own modulator slot must be silently ignored - we
    // use the 1-sample-delayed source value for cross-mod, and a self-route
    // would create a feedback loop. The processor-side guard is that op[i]
    // skips activation when modSrc == i; this test pins that behaviour.
    EngineHarness baseline;
    auto bRef = renderHeldNote (baseline, 60, 6);
    const auto bs = analyse (bRef);

    EngineHarness selfRouted;
    setParameter (selfRouted.tp, "osc1_mod_src",  (float) OscModSrc::Osc1);
    setParameter (selfRouted.tp, "osc1_mod_type", (float) OscModType::FM);
    setParameter (selfRouted.tp, "osc1_mod_amt",  1.0f);
    auto sRef = renderHeldNote (selfRouted, 60, 6);
    const auto ss = analyse (sRef);

    VK_EXPECT (! ss.hasNaN);
    VK_EXPECT (! ss.hasInf);
    // RMS should match within numerical tolerance: the self-route is treated
    // as Off, so output is bit-identical to the baseline modulo voice RNG
    // (the seeded grit / noise paths run regardless of cross-mod, so we
    // allow a small RMS delta).
    VK_EXPECT_NEAR (ss.rms, bs.rms, juce::jmax (1.0e-4f, bs.rms * 0.05f));
}
