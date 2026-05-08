// Parameter registry tests. Targets the public API in Parameters.h:
//   * createLayout() declares one parameter per Spec, in registry order
//   * cache() populates every std::atomic<float>* in SynthParams
//   * findSpec() / exists() agree, and cover every cached pointer
//   * declared defaults match what the APVTS hands out
//
// The PresetLint console app already validates the JSON-side of preset data
// against the registry. These tests cover the C++-side: nothing in the engine
// code can read a parameter that isn't in the registry, and no registry entry
// can leak an unbound atomic pointer.
#include "TestRunner.h"
#include "TestSupport.h"

using namespace VKTest;

VK_TEST (Parameters_RegistryNonEmpty)
{
    const auto& specs = Parameters::allSpecs();
    VK_EXPECT_GT ((int) specs.size(), 0);
    // Sanity bound - if this trips after a feature add, raise it; if the
    // count drops sharply something got lost.
    VK_EXPECT_GT ((int) specs.size(), 100);
}

VK_TEST (Parameters_AllSpecIdsUnique)
{
    juce::StringArray seen;
    for (const auto& s : Parameters::allSpecs())
    {
        VK_EXPECT_MSG (! seen.contains (s.id),
                       juce::String ("duplicate parameter id ") + s.id);
        seen.add (s.id);
    }
}

VK_TEST (Parameters_FindSpecRoundTrip)
{
    for (const auto& s : Parameters::allSpecs())
    {
        const auto* found = Parameters::findSpec (s.id);
        VK_REQUIRE (found != nullptr);
        VK_EXPECT_EQ (found->id, s.id);
        VK_EXPECT (Parameters::exists (s.id));
    }
    VK_EXPECT (Parameters::findSpec ("definitely_not_a_parameter") == nullptr);
    VK_EXPECT (! Parameters::exists ("definitely_not_a_parameter"));
}

VK_TEST (Parameters_LayoutCreatesOnePerSpec)
{
    TestProcessor tp;
    const auto& specs = Parameters::allSpecs();
    int hits = 0;
    for (const auto& s : specs)
        if (tp.apvts.getParameter (s.id) != nullptr)
            ++hits;
    VK_EXPECT_EQ (hits, (int) specs.size());
}

VK_TEST (Parameters_CacheBindsAllAtomicPointers)
{
    TestProcessor tp;

    // Walk every Spec and make sure the apvts can hand back a raw pointer
    // matching it. cache() guarantees one atomic per id; if any entry fails
    // here it means the registry's bind closure didn't wire that field.
    for (const auto& s : Parameters::allSpecs())
    {
        auto* raw = tp.apvts.getRawParameterValue (s.id);
        VK_EXPECT_MSG (raw != nullptr,
                       juce::String ("missing raw parameter for ") + s.id);
    }

    // Spot-check a handful of explicitly named SynthParams pointers - if these
    // were left null after cache(), the engine would crash on first block.
    VK_EXPECT (tp.params.fCut != nullptr);
    VK_EXPECT (tp.params.fRes != nullptr);
    VK_EXPECT (tp.params.aA   != nullptr);
    VK_EXPECT (tp.params.aR   != nullptr);
    VK_EXPECT (tp.params.gain != nullptr);
    VK_EXPECT (tp.params.osc[0].on != nullptr);
    VK_EXPECT (tp.params.osc[1].level != nullptr);
    VK_EXPECT (tp.params.osc[2].position != nullptr);
    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        VK_EXPECT (tp.params.macroVal[m]  != nullptr);
        VK_EXPECT (tp.params.macroDest[m] != nullptr);
        VK_EXPECT (tp.params.macroAmt[m]  != nullptr);
    }
}

VK_TEST (Parameters_DefaultsMatchSpecs)
{
    TestProcessor tp;
    resetParametersToDefaults (tp);

    for (const auto& s : Parameters::allSpecs())
    {
        auto* raw = tp.apvts.getRawParameterValue (s.id);
        VK_REQUIRE (raw != nullptr);
        const float v = raw->load();
        switch (s.kind)
        {
            case Parameters::Spec::Kind::Float:
                VK_EXPECT_NEAR (v, s.defaultValue, 1.0e-3f);
                break;
            case Parameters::Spec::Kind::Int:
                VK_EXPECT_EQ ((int) std::round (v), (int) s.defaultValue);
                break;
            case Parameters::Spec::Kind::Bool:
                // bool can't stream into juce::String, so phrase the
                // comparison through VK_EXPECT (no formatting on failure).
                VK_EXPECT ((v >= 0.5f) == (s.defaultValue >= 0.5f));
                break;
            case Parameters::Spec::Kind::Choice:
                VK_EXPECT_EQ ((int) std::round (v), (int) s.defaultValue);
                break;
        }
    }
}

VK_TEST (Parameters_ChoiceSpecsAreNonEmpty)
{
    for (const auto& s : Parameters::allSpecs())
    {
        if (s.kind != Parameters::Spec::Kind::Choice) continue;
        VK_EXPECT_MSG (s.choices.size() > 0,
                       juce::String ("choice parameter ") + s.id + " has empty choice list");
        VK_EXPECT_MSG ((int) s.defaultValue >= 0 && (int) s.defaultValue < s.choices.size(),
                       juce::String ("choice default for ") + s.id + " out of range");
    }
}

VK_TEST (Parameters_FloatRangesAreNonDegenerate)
{
    for (const auto& s : Parameters::allSpecs())
    {
        if (s.kind != Parameters::Spec::Kind::Float) continue;
        VK_EXPECT_MSG (s.floatRange.start < s.floatRange.end,
                       juce::String ("float range is degenerate for ") + s.id);
        VK_EXPECT_MSG (s.defaultValue >= s.floatRange.start && s.defaultValue <= s.floatRange.end,
                       juce::String ("float default for ") + s.id + " outside declared range");
    }
}

VK_TEST (Parameters_IntRangesAreSane)
{
    for (const auto& s : Parameters::allSpecs())
    {
        if (s.kind != Parameters::Spec::Kind::Int) continue;
        VK_EXPECT_MSG (s.intMin <= s.intMax,
                       juce::String ("int range is degenerate for ") + s.id);
        const int d = (int) s.defaultValue;
        VK_EXPECT_MSG (d >= s.intMin && d <= s.intMax,
                       juce::String ("int default for ") + s.id + " outside declared range");
    }
}

VK_TEST (Parameters_SetParameterTakesEffect)
{
    TestProcessor tp;
    resetParametersToDefaults (tp);

    // Pick a couple of well-known parameters and verify the cached pointer
    // observes a write. This exercises the entire Parameters::cache pathway.
    VK_REQUIRE (setParameter (tp, "f_cut", 5000.0f));
    VK_EXPECT_NEAR (tp.params.fCut->load(), 5000.0f, 1.0f);

    VK_REQUIRE (setParameter (tp, "gain", -3.0f));
    VK_EXPECT_NEAR (tp.params.gain->load(), -3.0f, 0.01f);

    VK_REQUIRE (setParameter (tp, "bend_range", 7.0f));
    VK_EXPECT_NEAR (tp.params.bendRange->load(), 7.0f, 0.01f);
}
