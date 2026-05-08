// Test runner entry point. Boots the JUCE message manager (the wavetable
// retirement queue + a couple of FX path dependencies expect one to exist),
// walks the registry, and prints a Catch2-ish summary. Exit code 0 on
// success, 1 on any test failure.
//
// CLI:
//   VaporKeyTests              # run everything
//   VaporKeyTests <substring>  # run only tests whose name contains <substring>
//   VaporKeyTests --list       # print the registered test names and exit
#include "TestRunner.h"

#include <iostream>

namespace VKTest
{

Registry& Registry::instance()
{
    static Registry r;
    return r;
}

void Registry::add (juce::String name, TestFn fn)
{
    entries.push_back ({ std::move (name), std::move (fn) });
}

std::vector<juce::String> Registry::names() const
{
    std::vector<juce::String> out;
    out.reserve (entries.size());
    for (const auto& e : entries) out.push_back (e.name);
    return out;
}

namespace
{
    Result* currentResult = nullptr;
}

namespace Current
{
    void begin (Result& r) { currentResult = &r; }
    void end()             { currentResult = nullptr; }

    void recordFailure (Failure f)
    {
        if (currentResult == nullptr) return;
        currentResult->passed = false;
        currentResult->failures.push_back (std::move (f));
    }

    bool hasFailed()
    {
        return currentResult != nullptr && ! currentResult->passed;
    }
}

int Registry::runAll (juce::StringRef filter)
{
    using Clock = std::chrono::steady_clock;

    int passed = 0, failed = 0, skipped = 0;
    std::vector<juce::String> failedNames;

    std::cout << "VaporKey test suite (" << entries.size() << " test"
              << (entries.size() == 1 ? "" : "s") << " registered)\n";

    for (auto& e : entries)
    {
        if (filter.isNotEmpty() && ! e.name.containsIgnoreCase (filter))
        {
            ++skipped;
            continue;
        }

        Result r;
        r.name = e.name;
        Current::begin (r);

        const auto start = Clock::now();
        try
        {
            e.fn();
        }
        catch (const std::exception& ex)
        {
            r.passed = false;
            r.failures.push_back ({ "<exception>", 0, "exception",
                                    juce::String ("std::exception: ") + ex.what() });
        }
        catch (...)
        {
            r.passed = false;
            r.failures.push_back ({ "<exception>", 0, "exception",
                                    "non-std::exception thrown" });
        }
        const auto end = Clock::now();
        r.durationMs = std::chrono::duration<double, std::milli> (end - start).count();

        Current::end();

        if (r.passed)
        {
            ++passed;
            std::cout << "  [PASS] " << r.name.toRawUTF8()
                      << "  (" << juce::String (r.durationMs, 1).toRawUTF8() << " ms)\n";
        }
        else
        {
            ++failed;
            failedNames.push_back (r.name);
            std::cout << "  [FAIL] " << r.name.toRawUTF8()
                      << "  (" << juce::String (r.durationMs, 1).toRawUTF8() << " ms)\n";
            for (const auto& f : r.failures)
            {
                std::cout << "       " << f.file.toRawUTF8() << ":" << f.line
                          << "  " << f.expr.toRawUTF8();
                if (f.message.isNotEmpty())
                    std::cout << "  -- " << f.message.toRawUTF8();
                std::cout << "\n";
            }
        }
    }

    std::cout << "\n";
    std::cout << "passed:  " << passed  << "\n";
    std::cout << "failed:  " << failed  << "\n";
    std::cout << "skipped: " << skipped << "\n";

    if (failed > 0)
    {
        std::cout << "\nfailed tests:\n";
        for (const auto& n : failedNames)
            std::cout << "  - " << n.toRawUTF8() << "\n";
        return 1;
    }
    return 0;
}

} // namespace VKTest

int main (int argc, char** argv)
{
    // Boot enough of JUCE for a message manager to exist - the
    // WavetableRetirementQueue's juce::Timer wants one. JUCE 8 dropped the
    // explicit non-GUI initialiser, so we use the GUI variant; juce_gui_basics
    // is already pulled in transitively by juce_audio_processors and tests
    // never open a real window.
    juce::ScopedJuceInitialiser_GUI initialiser;

    juce::String filter;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--list" || a == "-l")
        {
            listOnly = true;
            continue;
        }
        if (a == "--help" || a == "-h")
        {
            std::cout << "Usage: VaporKeyTests [--list] [<filter-substring>]\n";
            return 0;
        }
        filter = a;
    }

    if (listOnly)
    {
        std::cout << "Registered tests:\n";
        for (const auto& n : VKTest::Registry::instance().names())
            std::cout << "  " << n.toRawUTF8() << "\n";
        return 0;
    }

    return VKTest::Registry::instance().runAll (filter);
}
