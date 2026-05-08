// Preset I/O tests. Covers the factory preset apply path (Presets.json ->
// APVTS), state save/restore (the host-facing serialisation surface), and
// the user preset XML round-trip (PresetStore::read/save).
//
// The PresetLint console app validates the JSON statically. These tests
// exercise the runtime: that every preset *applies* without throwing, that
// applying then re-applying defaults isn't lossy, and that save/load is a
// fixed point.
#include "TestRunner.h"
#include "TestSupport.h"

#include "../Presets.h"
#include "../PresetStore.h"

#include <map>

using namespace VKTest;

namespace
{
    // Capture every parameter id -> value into a flat map for comparison.
    std::map<juce::String, float> snapshot (TestProcessor& tp)
    {
        std::map<juce::String, float> out;
        for (const auto& s : Parameters::allSpecs())
            if (auto* raw = tp.apvts.getRawParameterValue (s.id))
                out[s.id] = raw->load();
        return out;
    }
}

VK_TEST (Presets_FactoryListNonEmpty)
{
    const auto& list = VKPresets::all();
    VK_EXPECT_GT ((int) list.size(), 0);

    // Every preset has a name and at least one parameter set.
    for (const auto& p : list)
    {
        VK_EXPECT (p.name.isNotEmpty());
        VK_EXPECT_GT ((int) p.values.size(), 0);
    }
}

VK_TEST (Presets_FactoryNamesUnique)
{
    juce::StringArray seen;
    for (const auto& p : VKPresets::all())
    {
        VK_EXPECT_MSG (! seen.contains (p.name),
                       juce::String ("duplicate factory preset name: ") + p.name);
        seen.add (p.name);
    }
}

VK_TEST (Presets_PresetStoreReportsCounts)
{
    VK_EXPECT_EQ (PresetStore::factoryPresetCount(), (int) VKPresets::all().size());
    VK_EXPECT_EQ (PresetStore::factoryPresetNames().size(),
                  (int) VKPresets::all().size());
}

VK_TEST (Presets_EveryFactoryPresetApplies)
{
    TestProcessor tp;
    const int total = PresetStore::factoryPresetCount();
    VK_REQUIRE (total > 0);

    for (int i = 0; i < total; ++i)
    {
        const bool ok = PresetStore::applyFactoryPreset (tp.apvts, tp, i);
        VK_EXPECT_MSG (ok, juce::String ("failed to apply preset index ") + juce::String (i));
        if (! ok) continue;

        // Sanity: the apply path should leave parameters in their declared
        // ranges. Out-of-range values would only happen if the JSON had a
        // typo - PresetLint catches that, but we re-confirm here at runtime.
        for (const auto& s : Parameters::allSpecs())
        {
            auto* raw = tp.apvts.getRawParameterValue (s.id);
            VK_REQUIRE (raw != nullptr);
            const float v = raw->load();
            switch (s.kind)
            {
                case Parameters::Spec::Kind::Float:
                    VK_EXPECT_MSG (v >= s.floatRange.start - 1.0e-3f
                                && v <= s.floatRange.end + 1.0e-3f,
                                   juce::String ("preset ") + juce::String (i)
                                       + " left " + s.id + " out of range: "
                                       + juce::String (v, 4));
                    break;
                case Parameters::Spec::Kind::Int:
                    VK_EXPECT_MSG ((int) std::round (v) >= s.intMin
                                && (int) std::round (v) <= s.intMax,
                                   juce::String ("preset ") + juce::String (i)
                                       + " left " + s.id + " out of int range");
                    break;
                case Parameters::Spec::Kind::Bool:
                {
                    const int rounded = (int) std::round (v);
                    VK_EXPECT (rounded == 0 || rounded == 1);
                    break;
                }
                case Parameters::Spec::Kind::Choice:
                    VK_EXPECT_MSG ((int) std::round (v) >= 0
                                && (int) std::round (v) < s.choices.size(),
                                   juce::String ("preset ") + juce::String (i)
                                       + " left " + s.id + " out of choice index");
                    break;
            }
        }
    }
}

VK_TEST (Presets_ApplyOutOfRangeIsRejected)
{
    TestProcessor tp;
    VK_EXPECT (! PresetStore::applyFactoryPreset (tp.apvts, tp, -1));
    const int n = PresetStore::factoryPresetCount();
    VK_EXPECT (! PresetStore::applyFactoryPreset (tp.apvts, tp, n));
    VK_EXPECT (! PresetStore::applyFactoryPreset (tp.apvts, tp, n + 1000));
}

VK_TEST (Presets_StateSaveRestoreRoundTrip)
{
    TestProcessor tp;

    // Tweak a few well-known parameters to specific values.
    setParameter (tp, "f_cut",  3500.0f);
    setParameter (tp, "f_res",  0.7f);
    setParameter (tp, "gain",  -3.0f);
    setParameter (tp, "delay",  0.42f);
    setParameter (tp, "arp_on", 1.0f);

    const auto before = snapshot (tp);

    juce::MemoryBlock state;
    tp.getStateInformation (state);

    // Reset and verify the snapshot diverged (so the round-trip is meaningful).
    resetParametersToDefaults (tp);
    const auto reset = snapshot (tp);
    // Use a tolerant compare instead of != on floats so -Wfloat-equal stays
    // happy; the divergence we tweaked (3500 vs default 12000) is far larger
    // than any rounding slop.
    VK_EXPECT_GT (std::abs (before.find ("f_cut")->second
                            - reset.find ("f_cut")->second), 100.0f);

    tp.setStateInformation (state.getData(), (int) state.getSize());
    const auto after = snapshot (tp);

    VK_EXPECT_EQ ((int) before.size(), (int) after.size());
    for (const auto& kv : before)
    {
        const auto it = after.find (kv.first);
        VK_REQUIRE (it != after.end());
        VK_EXPECT_NEAR (kv.second, it->second, 1.0e-3f);
    }
}

VK_TEST (Presets_FactoryApplyIsDeterministic)
{
    // Apply preset 0 twice with a reset in between; both should produce the
    // same APVTS state.
    TestProcessor tp;
    PresetStore::applyFactoryPreset (tp.apvts, tp, 0);
    const auto first = snapshot (tp);

    resetParametersToDefaults (tp);
    PresetStore::applyFactoryPreset (tp.apvts, tp, 0);
    const auto second = snapshot (tp);

    for (const auto& kv : first)
    {
        const auto it = second.find (kv.first);
        VK_REQUIRE (it != second.end());
        VK_EXPECT_NEAR (kv.second, it->second, 1.0e-3f);
    }
}

VK_TEST (Presets_UserPresetXmlRoundTrip)
{
    // The user preset format is the same XML the host state save/restore
    // produces (PresetStore::saveUserPreset / readUserPresetXml are thin
    // file-IO wrappers around apvts.copyState().createXml() and
    // ValueTree::fromXml). Round-trip the XML in memory to exercise the
    // same path without writing to the user's app-data directory.
    TestProcessor tp;
    setParameter (tp, "f_cut",  6500.0f);
    setParameter (tp, "f_res",  0.55f);
    setParameter (tp, "delay",  0.33f);

    auto xml = tp.apvts.copyState().createXml();
    VK_REQUIRE (xml != nullptr);

    resetParametersToDefaults (tp);
    tp.apvts.replaceState (juce::ValueTree::fromXml (*xml));

    VK_EXPECT_NEAR (tp.params.fCut->load(), 6500.0f, 1.0f);
    VK_EXPECT_NEAR (tp.params.fRes->load(), 0.55f, 0.01f);
    VK_EXPECT_NEAR (tp.params.delay->load(), 0.33f, 0.01f);
}
