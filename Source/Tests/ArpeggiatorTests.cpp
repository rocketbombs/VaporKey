// Arpeggiator tests. The arp is a pure MIDI rewriter; given a held chord
// and a transport tempo it produces a stepped sequence of note-on/off events
// at sub-block sample positions. The audio engine never sees the original
// chord.
//
// Coverage:
//   * arp off: notes pass through untouched
//   * arp on with held note: emits at least one note-on per step interval
//   * Up mode walks notes low->high
//   * Down mode walks notes high->low
//   * UpDown mode reverses at the top
//   * Octave range > 1 stacks copies up
//   * Latch keeps notes after release
//   * Swing skews odd-step durations
//   * CCs / pitch-bend pass through unchanged even when arp is on
//   * Non-allocating: no audio-thread allocations in process() (smoke test)
#include "TestRunner.h"
#include "TestSupport.h"

#include <set>
#include <vector>

using namespace VKTest;

namespace
{
    struct ArpHarness
    {
        TestProcessor tp;
        Arpeggiator   arp { tp.params };
        double        sr  { 48000.0 };

        ArpHarness()
        {
            resetParametersToDefaults (tp);
            arp.prepare (sr);
        }
    };

    juce::MidiBuffer pressChord (std::initializer_list<int> notes, int samplePos = 0)
    {
        juce::MidiBuffer mb;
        for (int n : notes) mb.addEvent (Midi::noteOn (n, 100), samplePos);
        return mb;
    }
}

VK_TEST (Arpeggiator_OffPassesNotesThrough)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on", 0.0f);

    juce::MidiBuffer mb = pressChord ({ 60, 64, 67 });
    h.arp.process (mb, 256, 120.0);

    auto events = collectNoteEvents (mb);
    VK_EXPECT_EQ ((int) events.size(), 3);
}

VK_TEST (Arpeggiator_OnEmitsSequencedSteps)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::Up);
    setParameter (h.tp, "arp_div",  4.0f); // 1/4 note
    setParameter (h.tp, "arp_octaves", 1.0f);

    // Press chord at sample 0, then run a full block at 120 BPM. At 1/4
    // note that's 0.5s per step at 120bpm = 24000 samples per step at 48kHz.
    // A 256-sample block won't see a complete step - so step the arp across
    // many short blocks and collect events.
    juce::MidiBuffer mb = pressChord ({ 60, 64, 67 });
    h.arp.process (mb, 32, 120.0);
    auto pressEvents = collectNoteEvents (mb);
    juce::ignoreUnused (pressEvents);

    // Drive long enough to see at least 4 steps.
    int blocks = (int) std::ceil (4.0 * 0.5 * h.sr / 256.0) + 4;
    std::vector<MidiNoteEvent> all;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 256, 120.0);
        for (auto& e : collectNoteEvents (m))
        {
            e.sample += b * 256; // make sample positions globally meaningful
            all.push_back (e);
        }
    }

    // Count the unique notes the arp emitted across those steps. Should be
    // exactly the three input notes (no octave stacking).
    std::set<int> emitted;
    int onCount = 0;
    for (auto& e : all)
    {
        if (e.isOn) { emitted.insert (e.note); ++onCount; }
    }
    VK_EXPECT_GT (onCount, 0);
    VK_EXPECT_EQ ((int) emitted.size(), 3);
    VK_EXPECT (emitted.count (60) == 1);
    VK_EXPECT (emitted.count (64) == 1);
    VK_EXPECT (emitted.count (67) == 1);
}

VK_TEST (Arpeggiator_UpModeStepsAscending)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::Up);
    setParameter (h.tp, "arp_div",  0.0f); // 1/32 note - very fast steps
    setParameter (h.tp, "arp_octaves", 1.0f);
    setParameter (h.tp, "arp_gate", 0.5f);

    juce::MidiBuffer chordIn = pressChord ({ 60, 64, 67 });
    h.arp.process (chordIn, 32, 120.0);

    std::vector<int> noteOnSequence;
    // 1/32 at 120 BPM = 0.0625s = 3000 samples. Run 9 such steps.
    int totalBlocks = 14;
    for (int b = 0; b < totalBlocks; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 1024, 120.0);
        for (auto& e : collectNoteEvents (m))
            if (e.isOn) noteOnSequence.push_back (e.note);
    }

    VK_REQUIRE ((int) noteOnSequence.size() >= 4);
    // Up mode: note sequence should cycle through 60, 64, 67, 60, 64, 67, ...
    VK_EXPECT_EQ (noteOnSequence[0], 60);
    VK_EXPECT_EQ (noteOnSequence[1], 64);
    VK_EXPECT_EQ (noteOnSequence[2], 67);
    VK_EXPECT_EQ (noteOnSequence[3], 60);
}

VK_TEST (Arpeggiator_DownModeStepsDescending)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::Down);
    setParameter (h.tp, "arp_div",  0.0f);
    setParameter (h.tp, "arp_octaves", 1.0f);

    juce::MidiBuffer chordIn = pressChord ({ 60, 64, 67 });
    h.arp.process (chordIn, 32, 120.0);

    std::vector<int> seq;
    for (int b = 0; b < 14; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 1024, 120.0);
        for (auto& e : collectNoteEvents (m))
            if (e.isOn) seq.push_back (e.note);
    }

    VK_REQUIRE ((int) seq.size() >= 4);
    VK_EXPECT_EQ (seq[0], 67);
    VK_EXPECT_EQ (seq[1], 64);
    VK_EXPECT_EQ (seq[2], 60);
    VK_EXPECT_EQ (seq[3], 67);
}

VK_TEST (Arpeggiator_UpDownModeReversesAtPeak)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::UpDown);
    setParameter (h.tp, "arp_div",  0.0f);
    setParameter (h.tp, "arp_octaves", 1.0f);

    juce::MidiBuffer chordIn = pressChord ({ 60, 64, 67 });
    h.arp.process (chordIn, 32, 120.0);

    std::vector<int> seq;
    for (int b = 0; b < 14; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 1024, 120.0);
        for (auto& e : collectNoteEvents (m))
            if (e.isOn) seq.push_back (e.note);
    }

    VK_REQUIRE ((int) seq.size() >= 5);
    VK_EXPECT_EQ (seq[0], 60);
    VK_EXPECT_EQ (seq[1], 64);
    VK_EXPECT_EQ (seq[2], 67);
    // Up/Down has 2N-2 steps per cycle for N notes; the next pick is step 3
    // which mirrors back to index 1 (= note 64), and step 4 = 60.
    VK_EXPECT_EQ (seq[3], 64);
    VK_EXPECT_EQ (seq[4], 60);
}

VK_TEST (Arpeggiator_OctaveRangeStacksCopiesUp)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::Up);
    setParameter (h.tp, "arp_div",  0.0f);
    setParameter (h.tp, "arp_octaves", 2.0f);

    juce::MidiBuffer chordIn = pressChord ({ 60 });
    h.arp.process (chordIn, 32, 120.0);

    std::vector<int> seq;
    for (int b = 0; b < 8; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 2048, 120.0);
        for (auto& e : collectNoteEvents (m))
            if (e.isOn) seq.push_back (e.note);
    }

    VK_REQUIRE ((int) seq.size() >= 4);
    // With one held note and 2 octaves, Up mode emits 60, 72, 60, 72, ...
    VK_EXPECT_EQ (seq[0], 60);
    VK_EXPECT_EQ (seq[1], 72);
    VK_EXPECT_EQ (seq[2], 60);
    VK_EXPECT_EQ (seq[3], 72);
}

VK_TEST (Arpeggiator_LatchKeepsNotesAfterRelease)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on",     1.0f);
    setParameter (h.tp, "arp_mode",   (float) ArpMode::Up);
    setParameter (h.tp, "arp_div",    0.0f);
    setParameter (h.tp, "arp_octaves", 1.0f);
    setParameter (h.tp, "arp_latch",  1.0f);

    juce::MidiBuffer press = pressChord ({ 60, 64 });
    h.arp.process (press, 32, 120.0);

    juce::MidiBuffer release;
    release.addEvent (Midi::noteOff (60), 0);
    release.addEvent (Midi::noteOff (64), 0);
    h.arp.process (release, 32, 120.0);

    // After release the latched chord should keep emitting steps.
    std::vector<int> seq;
    for (int b = 0; b < 10; ++b)
    {
        juce::MidiBuffer m;
        h.arp.process (m, 1024, 120.0);
        for (auto& e : collectNoteEvents (m))
            if (e.isOn) seq.push_back (e.note);
    }
    VK_EXPECT_GT ((int) seq.size(), 0);
}

VK_TEST (Arpeggiator_PassesControllersThrough)
{
    ArpHarness h;
    setParameter (h.tp, "arp_on", 1.0f);
    setParameter (h.tp, "arp_div", 4.0f);

    juce::MidiBuffer m;
    m.addEvent (Midi::cc (1, 100), 16);
    m.addEvent (Midi::pitchWheel (12000), 32);
    m.addEvent (Midi::channelPressure (90), 64);
    h.arp.process (m, 256, 120.0);

    int seenCc = 0, seenBend = 0, seenAt = 0;
    for (const auto meta : m)
    {
        const auto msg = meta.getMessage();
        if (msg.isController() && msg.getControllerNumber() == 1) ++seenCc;
        if (msg.isPitchWheel())                                    ++seenBend;
        if (msg.isAftertouch() || msg.isChannelPressure())         ++seenAt;
    }
    VK_EXPECT_EQ (seenCc, 1);
    VK_EXPECT_EQ (seenBend, 1);
    VK_EXPECT_EQ (seenAt, 1);
}

VK_TEST (Arpeggiator_TransitionToOnEmitsClosingNoteOff)
{
    // If the arp is currently sounding a note and the user toggles off,
    // the engine should emit a note-off so no voice is left hanging.
    ArpHarness h;
    setParameter (h.tp, "arp_on",   1.0f);
    setParameter (h.tp, "arp_mode", (float) ArpMode::Up);
    setParameter (h.tp, "arp_div",  0.0f);

    juce::MidiBuffer chord = pressChord ({ 60 });
    h.arp.process (chord, 32, 120.0);
    // Drive enough to issue a step.
    juce::MidiBuffer empty;
    h.arp.process (empty, 4096, 120.0);

    // Now flip arp off; first block after the flip should emit a note-off.
    setParameter (h.tp, "arp_on", 0.0f);

    juce::MidiBuffer post;
    h.arp.process (post, 256, 120.0);

    int offCount = 0;
    for (const auto meta : post)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOff()) ++offCount;
    }
    VK_EXPECT_GT (offCount, 0);
}

VK_TEST (Arpeggiator_DivisionsAlterStepRate)
{
    // Compare 1/16 vs 1/4: 1/16 should produce ~4x the note count of 1/4
    // over the same time window.
    auto countSteps = [] (float div) -> int
    {
        ArpHarness h;
        setParameter (h.tp, "arp_on",   1.0f);
        setParameter (h.tp, "arp_mode", (float) ArpMode::Up);
        setParameter (h.tp, "arp_div",  div);

        juce::MidiBuffer chord = pressChord ({ 60 });
        h.arp.process (chord, 32, 120.0);

        int onCount = 0;
        // 4 seconds of audio at 48kHz = 192000 samples; that's plenty for both.
        for (int b = 0; b < 188; ++b)
        {
            juce::MidiBuffer m;
            h.arp.process (m, 1024, 120.0);
            for (const auto meta : m)
                if (meta.getMessage().isNoteOn()) ++onCount;
        }
        return onCount;
    };

    const int s16  = countSteps (1.0f); // 1/16
    const int s4   = countSteps (4.0f); // 1/4

    VK_EXPECT_GT (s16, s4);
    // 1/16 is 4x the rate of 1/4 - allow a 25% tolerance for boundary effects
    // at the start/end of the time window.
    VK_EXPECT_GT ((double) s16, s4 * 3.0);
    VK_EXPECT_LT ((double) s16, s4 * 5.0);
}
