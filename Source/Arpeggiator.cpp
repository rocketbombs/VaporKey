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

    // Transition into/out of arp mode: clear pending state cleanly.
    if (on != wasOn)
    {
        if (currentNote >= 0)
            pass.addEvent (juce::MidiMessage::noteOff (currentChan, currentNote), 0);
        currentNote = -1;
        samplesToOff = -1;
        samplesToStep = 0.0;
        stepIdx = 0;
        octOffset = 0;
        if (! on) { held.clearQuick(); latched.clearQuick(); }
        wasOn = on;
    }

    if (! on)
    {
        // Pass note events through untouched.
        for (int i = 0; i < noteEvents.size(); ++i)
            pass.addEvent (noteEvents.getReference (i), noteSamples.getReference (i));
        midi.swapWith (pass);
        return;
    }

    // ---- Arp on ----
    // Step duration in samples, derived from tempo + sync division.
    const int divIdx = (int) (params.arpDiv->load() + 0.5f);
    const double beats = syncDivToBeats (divIdx);
    const double bpm   = juce::jmax (20.0, currentBpm);
    const double stepSamples = juce::jmax (4.0, beats * 60.0 / bpm * sr);

    const int   mode    = (int) (params.arpMode->load() + 0.5f);
    const int   numOct  = juce::jlimit (1, 4, (int) params.arpOctaves->load());
    const float gate    = juce::jlimit (0.05f, 1.0f, params.arpGate->load());
    const float swing   = juce::jlimit (0.0f, 0.5f, params.arpSwing->load());

    // Walk the block, advancing time and emitting events at sub-block points.
    int cursor = 0;
    int eventIdx = 0;

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
