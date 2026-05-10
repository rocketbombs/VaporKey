#include "Arpeggiator.h"
#include <algorithm>

Arpeggiator::Arpeggiator (SynthParams& sp)
    : params (sp)
{
    // Decorrelate per-instance RNG. Without this every plugin instance starts
    // its arpeggiator with the same JUCE default seed, so multiple instances
    // produce identical random streams - which sums coherently and sounds
    // buzzy / aliased.
    rng.setSeedRandomly();
}

void Arpeggiator::prepare (double sampleRate)
{
    sr = sampleRate;

    // Reserve generous capacity once so the audio thread never reallocates
    // these scratch buffers (the source of multi-instance host freezes).
    passBuf.ensureSize (8192);
    noteEventsBuf.ensureStorageAllocated (256);
    noteSamplesBuf.ensureStorageAllocated (256);
    activeBuf.ensureStorageAllocated (32);
    orderedBuf.ensureStorageAllocated (32);
    held.ensureStorageAllocated (32);
    latched.ensureStorageAllocated (32);
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, double currentBpm)
{
    const bool on    = *params.arpOn > 0.5f;
    const bool latch = *params.arpLatch > 0.5f;

    // Step duration in samples, derived from tempo + sync division. Computed
    // up-front so the arp-on transition handler can use it to defer the first
    // step by one full cycle (otherwise pressing a chord at the same instant
    // the arp turns on fires the lowest note immediately, before the user-
    // perceived "next beat", and skews every ordered-mode sequence by one).
    const int    divIdx       = (int) (params.arpDiv->load() + 0.5f);
    const double beats        = syncDivToBeats (divIdx);
    const double bpm          = juce::jmax (20.0, currentBpm);
    const double stepSamples  = juce::jmax (4.0, beats * 60.0 / bpm * sr);

    auto& pass        = passBuf;
    auto& noteEvents  = noteEventsBuf;
    auto& noteSamples = noteSamplesBuf;
    pass.clear();
    noteEvents.clearQuick();
    noteSamples.clearQuick();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOnOrOff())
        {
            noteEvents.add (msg);
            noteSamples.add (meta.samplePosition);
        }
        else
        {
            pass.addEvent (msg, meta.samplePosition);
        }
    }

    // Update held/latched lists from a single note event. Used both during
    // arp-on stepping (interleaved with cursor advance) and arp-off
    // pass-through, so `held` always reflects what the user is physically
    // holding and the OFF -> ON transition can silence the synth voices that
    // pass-through left ringing.
    auto applyHeldNoteEvent = [this, latch] (const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            // Latch behavior: a fresh press while no notes physically held should
            // start a new chord (clear latched buffer first).
            if (latch && held.isEmpty())
                latched.clearQuick();

            const int n = msg.getNoteNumber();
            for (int j = held.size(); --j >= 0;)
                if (held.getReference (j).note == n) held.remove (j);
            held.add ({ n, msg.getVelocity() });

            if (latch)
            {
                for (int j = latched.size(); --j >= 0;)
                    if (latched.getReference (j).note == n) latched.remove (j);
                latched.add ({ n, msg.getVelocity() });
            }
        }
        else if (msg.isNoteOff())
        {
            const int n = msg.getNoteNumber();
            for (int j = held.size(); --j >= 0;)
                if (held.getReference (j).note == n) held.remove (j);
        }
    };

    // Transition into/out of arp mode: clean up pending state and re-route
    // physically-held notes between the synth and the arp's pool so toggling
    // never leaves a stuck voice droning under the new mode.
    if (on != wasOn)
    {
        // Kill any in-flight arp-emitted note (its scheduling is about to
        // be invalidated either way).
        if (currentNote >= 0)
            pass.addEvent (juce::MidiMessage::noteOff (currentChan, currentNote), 0);
        currentNote = -1;
        samplesToOff = -1;
        // Defer the first step by one full cycle on power-on so step 0 fires
        // strictly after the chord is registered (matches the rest of the
        // sequence: chord visible, then step, not the other way round).
        samplesToStep = on ? stepSamples : 0.0;
        stepIdx = 0;
        octOffset = 0;

        if (on)
        {
            // OFF -> ON: notes the user is still holding had their note-ons
            // pass through directly to the synth in OFF mode, so synth voices
            // are sustaining for those keys. Send matching note-offs now so
            // the arp can take over the held set cleanly without leaving
            // stuck voices droning under the sequence.
            for (const auto& h : held)
                pass.addEvent (juce::MidiMessage::noteOff (currentChan, h.note), 0);

            // Latch was empty going in (cleared by the prior ON -> OFF
            // transition); seed it from the currently-held set so a user who
            // enabled latch+arp while holding keys gets the held chord
            // latched, the same way it would if they had pressed those keys
            // after the arp turned on.
            if (latch)
            {
                latched.clearQuick();
                for (const auto& h : held)
                    latched.add (h);
            }
        }
        else
        {
            // ON -> OFF: arp had been suppressing pass-through for held keys,
            // so the synth has no voices for them. Re-emit note-ons so the
            // user's physically-held keys come back as direct voices; future
            // note-offs in OFF mode (when the user releases) will release
            // them normally. Latched notes were never physically held, so
            // discard the latch buffer rather than re-pressing them.
            for (const auto& h : held)
            {
                const int vel = juce::jlimit (1, 127, h.velocity > 0 ? h.velocity : 100);
                pass.addEvent (juce::MidiMessage::noteOn (currentChan, h.note, (juce::uint8) vel), 0);
            }
            latched.clearQuick();
        }
        wasOn = on;
    }

    if (! on)
    {
        // Pass note events through, but also keep `held` updated so the next
        // OFF -> ON transition knows which synth voices need silencing.
        for (int i = 0; i < noteEvents.size(); ++i)
        {
            applyHeldNoteEvent (noteEvents.getReference (i));
            pass.addEvent (noteEvents.getReference (i), noteSamples.getReference (i));
        }
        midi.swapWith (pass);
        return;
    }

    // ---- Arp on ---- (stepSamples already computed above)

    const int   mode    = (int) (params.arpMode->load() + 0.5f);
    const int   numOct  = juce::jlimit (1, 4, (int) params.arpOctaves->load());
    const float gate    = juce::jlimit (0.05f, 1.0f, params.arpGate->load());
    const float swing   = juce::jlimit (0.0f, 0.5f, params.arpSwing->load());

    // Walk the block, advancing time and emitting events at sub-block points.
    int cursor = 0;
    int eventIdx = 0;

    auto pickStepNote = [&] (const juce::Array<Held>& source) -> Held
    {
        // Sorted copy for ordered modes - reuse a member buffer to avoid heap
        // allocation every step boundary.
        auto& ordered = orderedBuf;
        ordered.clearQuick();
        ordered.addArray (source);
        std::sort (ordered.begin(), ordered.end(),
                   [] (const Held& a, const Held& b) { return a.note < b.note; });

        const int N = ordered.size();
        if (N == 0) return { -1, 0 };

        const int totalSteps = N * numOct;

        auto wrapStep = [&] (int s)
        {
            const int m = s % juce::jmax (1, totalSteps);
            return m < 0 ? m + totalSteps : m;
        };

        switch (mode)
        {
            case ArpMode::Up:
            {
                const int s = wrapStep (stepIdx);
                octOffset = (s / N) * 12;
                return { ordered.getReference (s % N).note + octOffset, ordered.getReference (s % N).velocity };
            }
            case ArpMode::Down:
            {
                const int s = wrapStep (stepIdx);
                const int rev = totalSteps - 1 - s;
                octOffset = (rev / N) * 12;
                return { ordered.getReference (rev % N).note + octOffset, ordered.getReference (rev % N).velocity };
            }
            case ArpMode::UpDown:
            {
                const int span = juce::jmax (1, 2 * totalSteps - 2);
                const int s    = ((stepIdx % span) + span) % span;
                const int idx  = (s < totalSteps) ? s : (span - s);
                octOffset = (idx / N) * 12;
                return { ordered.getReference (idx % N).note + octOffset, ordered.getReference (idx % N).velocity };
            }
            case ArpMode::DownUp:
            {
                const int span = juce::jmax (1, 2 * totalSteps - 2);
                const int s    = ((stepIdx % span) + span) % span;
                const int idxFwd = (s < totalSteps) ? s : (span - s);
                const int idx    = totalSteps - 1 - idxFwd;
                octOffset = (idx / N) * 12;
                return { ordered.getReference (idx % N).note + octOffset, ordered.getReference (idx % N).velocity };
            }
            case ArpMode::AsPlayed:
            {
                const int s = wrapStep (stepIdx);
                octOffset = (s / N) * 12;
                return { source.getReference (s % N).note + octOffset, source.getReference (s % N).velocity };
            }
            case ArpMode::Random:
            {
                const int oct = rng.nextInt (numOct);
                const int idx = rng.nextInt (N);
                return { ordered.getReference (idx).note + oct * 12, ordered.getReference (idx).velocity };
            }
        }
        return { -1, 0 };
    };

    while (cursor < numSamples)
    {
        // Apply any input note events at or before the cursor that we haven't yet processed.
        while (eventIdx < noteEvents.size() && noteSamples.getReference (eventIdx) <= cursor)
        {
            applyHeldNoteEvent (noteEvents.getReference (eventIdx));
            ++eventIdx;
        }

        // Active source for picking: physical held + (optionally) latched
        // extras. Reuse a member buffer so the inner loop allocates nothing.
        auto& activeSource = activeBuf;
        activeSource.clearQuick();
        activeSource.addArray (held);
        if (latch)
        {
            for (const auto& e : latched)
            {
                bool found = false;
                for (const auto& a : activeSource) if (a.note == e.note) { found = true; break; }
                if (! found) activeSource.add (e);
            }
        }

        // How many samples until the next event boundary?
        double samplesToNextStep = samplesToStep;
        const double samplesToInputEvent = (eventIdx < noteEvents.size())
            ? (double) (noteSamples.getReference (eventIdx) - cursor) : 1e18;
        const double samplesToOffD = (samplesToOff >= 0) ? (double) samplesToOff : 1e18;

        const double dt = std::min ({ (double) (numSamples - cursor),
                                       samplesToNextStep,
                                       samplesToInputEvent,
                                       samplesToOffD });

        cursor += (int) dt;
        samplesToStep -= dt;
        if (samplesToOff >= 0) samplesToOff -= (int) dt;

        // Fire scheduled note-off if it's now due.
        if (samplesToOff == 0 && currentNote >= 0)
        {
            pass.addEvent (juce::MidiMessage::noteOff (currentChan, currentNote), juce::jmin (cursor, numSamples - 1));
            currentNote = -1;
            samplesToOff = -1;
        }

        // Step boundary?
        if (samplesToStep <= 0.0)
        {
            // End any still-sounding note (e.g. when gate == 1.0 it overlaps).
            if (currentNote >= 0)
            {
                pass.addEvent (juce::MidiMessage::noteOff (currentChan, currentNote), juce::jmin (cursor, numSamples - 1));
                currentNote = -1;
                samplesToOff = -1;
            }

            if (! activeSource.isEmpty())
            {
                const Held pick = pickStepNote (activeSource);
                if (pick.note >= 0)
                {
                    const int note = juce::jlimit (0, 127, pick.note);
                    const int vel  = juce::jlimit (1, 127, pick.velocity > 0 ? pick.velocity : 100);
                    pass.addEvent (juce::MidiMessage::noteOn (currentChan, note, (juce::uint8) vel),
                                   juce::jmin (cursor, numSamples - 1));
                    currentNote = note;
                    samplesToOff = juce::jmax (1, (int) (stepSamples * gate));
                }
            }

            // Schedule next step. Apply swing on odd steps (delay them).
            const bool oddStep = (stepIdx & 1) != 0;
            const double stepDur = stepSamples * (oddStep ? (1.0 + swing) : (1.0 - swing));
            samplesToStep += stepDur;
            ++stepIdx;
        }
    }

    // Apply any remaining input events that landed past the loop's tail.
    while (eventIdx < noteEvents.size())
    {
        applyHeldNoteEvent (noteEvents.getReference (eventIdx));
        ++eventIdx;
    }

    midi.swapWith (pass);
}
