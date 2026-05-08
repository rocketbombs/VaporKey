// Tiny self-contained test framework for VaporKey. Three things motivated
// rolling our own instead of pulling Catch2 / GoogleTest:
//   * the only existing build dependency is JUCE; CMake stays trivial to read.
//   * the parser tests need direct access to internal helpers (Parameters,
//     Wavetable) that are already linked into the same translation units.
//   * test output goes through std::cout/cerr exactly the way the existing
//     PresetLint console app does, so CI logs read the same.
//
// Tests register themselves at static-init time via VK_TEST(name); main() in
// TestRunner.cpp walks the registry, runs each, and exits non-zero on the
// first suite that records a failure. There's no fixtures/parameterisation
// machinery on purpose - we don't need it, and the absence keeps the
// framework readable.
#pragma once

#include <JuceHeader.h>

#include <chrono>
#include <functional>
#include <string>

namespace VKTest
{

struct Failure
{
    juce::String file;
    int          line { 0 };
    juce::String expr;
    juce::String message;
};

struct Result
{
    juce::String       name;
    bool               passed { true };
    std::vector<Failure> failures;
    double             durationMs { 0.0 };
};

class Registry
{
public:
    using TestFn = std::function<void()>;

    static Registry& instance();

    void                add (juce::String name, TestFn fn);
    int                 runAll (juce::StringRef filter);
    std::vector<juce::String> names() const;

private:
    struct Entry
    {
        juce::String name;
        TestFn       fn;
    };
    std::vector<Entry> entries;
};

// Set / read the per-test failure buffer. The test body never sees these
// directly - the EXPECT_* macros do. recordFailure() records and returns,
// the test continues running so a single test can surface multiple failures.
namespace Current
{
    void  begin (Result& r);
    void  end();
    void  recordFailure (Failure f);
    bool  hasFailed();
}

struct AutoRegister
{
    AutoRegister (juce::String name, Registry::TestFn fn)
    {
        Registry::instance().add (std::move (name), std::move (fn));
    }
};

} // namespace VKTest

#define VK_TEST_CONCAT_INNER(a, b) a##b
#define VK_TEST_CONCAT(a, b) VK_TEST_CONCAT_INNER(a, b)

#define VK_TEST(NAME)                                                          \
    static void VK_TEST_CONCAT (vk_test_body_, NAME) ();                       \
    static ::VKTest::AutoRegister VK_TEST_CONCAT (vk_test_reg_, NAME) (        \
        #NAME, &VK_TEST_CONCAT (vk_test_body_, NAME));                         \
    static void VK_TEST_CONCAT (vk_test_body_, NAME) ()

#define VK_EXPECT(EXPR)                                                        \
    do                                                                         \
    {                                                                          \
        if (! (EXPR))                                                          \
            ::VKTest::Current::recordFailure ({ __FILE__, __LINE__, #EXPR, {} }); \
    } while (false)

#define VK_EXPECT_MSG(EXPR, MSG)                                               \
    do                                                                         \
    {                                                                          \
        if (! (EXPR))                                                          \
            ::VKTest::Current::recordFailure ({ __FILE__, __LINE__, #EXPR,     \
                                                juce::String (MSG) });         \
    } while (false)

#define VK_EXPECT_NEAR(A, B, EPS)                                              \
    do                                                                         \
    {                                                                          \
        const double vk_a = (double) (A);                                      \
        const double vk_b = (double) (B);                                      \
        const double vk_e = (double) (EPS);                                    \
        if (std::abs (vk_a - vk_b) > vk_e)                                     \
        {                                                                      \
            juce::String msg;                                                  \
            msg << "got " << vk_a << ", expected " << vk_b                     \
                << " (eps=" << vk_e << ")";                                    \
            ::VKTest::Current::recordFailure (                                 \
                { __FILE__, __LINE__, #A " ~= " #B, msg });                    \
        }                                                                      \
    } while (false)

#define VK_EXPECT_EQ(A, B)                                                     \
    do                                                                         \
    {                                                                          \
        const auto vk_a = (A);                                                 \
        const auto vk_b = (B);                                                 \
        if (! (vk_a == vk_b))                                                  \
        {                                                                      \
            juce::String msg;                                                  \
            msg << "got " << vk_a << ", expected " << vk_b;                    \
            ::VKTest::Current::recordFailure (                                 \
                { __FILE__, __LINE__, #A " == " #B, msg });                    \
        }                                                                      \
    } while (false)

#define VK_EXPECT_LT(A, B)                                                     \
    do                                                                         \
    {                                                                          \
        const auto vk_a = (A);                                                 \
        const auto vk_b = (B);                                                 \
        if (! (vk_a < vk_b))                                                   \
        {                                                                      \
            juce::String msg;                                                  \
            msg << "got " << vk_a << ", expected < " << vk_b;                  \
            ::VKTest::Current::recordFailure (                                 \
                { __FILE__, __LINE__, #A " < " #B, msg });                     \
        }                                                                      \
    } while (false)

#define VK_EXPECT_GT(A, B)                                                     \
    do                                                                         \
    {                                                                          \
        const auto vk_a = (A);                                                 \
        const auto vk_b = (B);                                                 \
        if (! (vk_a > vk_b))                                                   \
        {                                                                      \
            juce::String msg;                                                  \
            msg << "got " << vk_a << ", expected > " << vk_b;                  \
            ::VKTest::Current::recordFailure (                                 \
                { __FILE__, __LINE__, #A " > " #B, msg });                     \
        }                                                                      \
    } while (false)

#define VK_REQUIRE(EXPR)                                                       \
    do                                                                         \
    {                                                                          \
        if (! (EXPR))                                                          \
        {                                                                      \
            ::VKTest::Current::recordFailure (                                 \
                { __FILE__, __LINE__, #EXPR, "REQUIRE failed - aborting test" }); \
            return;                                                            \
        }                                                                      \
    } while (false)
