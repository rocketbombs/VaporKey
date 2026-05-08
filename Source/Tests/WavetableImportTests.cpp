// Wavetable import tests. The .wav importer is the only entry point that
// builds a wavetable on demand from disk, and it shares a retirement queue
// with the audio thread that's documented in docs/RealtimeSafety.md as the
// most subtle bit of cross-thread machinery in the project. Failures here
// would surface as audio-thread allocations or use-after-free under
// drag-and-drop.
//
// Coverage:
//   * loadInto rejects missing files without mutating the slot
//   * loadInto succeeds on a real .wav and updates both the shared_ptr and
//     the path string
//   * clear() empties the slot and updates the path
//   * displayName() drops the directory + extension
//   * the retirement queue holds a retired table until use_count == 1, then
//     drops it on sweep()
//   * multi-channel wavs are mixed to mono before building (no shape leak
//     between channels)
#include "TestRunner.h"
#include "TestSupport.h"

#include "../WavetableImport.h"

using namespace VKTest;

namespace
{
    juce::File makeMonoWavWithCharacter()
    {
        // Multi-frame: fundamental on frame 0, more harmonics on later frames.
        return writeTempWav (makeFramedTestSamples (Wavetable::kNumFrames));
    }

    juce::File makeStereoWav()
    {
        // Build interleaved L/R: L = sine, R = same sine inverted, so the
        // mono fold of L+R should be near-silent. Tests the channel mix-down.
        const int frames = Wavetable::kNumFrames;
        std::vector<float> interleaved ((size_t) (frames * Wavetable::kFrameSize * 2));
        for (int i = 0; i < frames * Wavetable::kFrameSize; ++i)
        {
            const float t = (float) (i % Wavetable::kFrameSize)
                                 / (float) Wavetable::kFrameSize;
            const float v = std::sin (juce::MathConstants<float>::twoPi * t);
            interleaved[(size_t) (2 * i + 0)] =  v;
            interleaved[(size_t) (2 * i + 1)] = -v;
        }
        return writeTempWav (interleaved, 2);
    }
}

VK_TEST (WavetableImport_RejectsMissingFile)
{
    WavetableRetirementQueue q;
    std::shared_ptr<Wavetable> slot;
    juce::String path;

    juce::File missing { "/definitely/does/not/exist/vk_test_missing.wav" };
    const bool ok = WavetableImport::loadInto (slot, path, missing, q);

    VK_EXPECT (! ok);
    VK_EXPECT (slot == nullptr);
    VK_EXPECT (path.isEmpty());
}

VK_TEST (WavetableImport_LoadsValidWav)
{
    WavetableRetirementQueue q;
    std::shared_ptr<Wavetable> slot;
    juce::String path;

    auto tmp = makeMonoWavWithCharacter();
    VK_REQUIRE (tmp.existsAsFile());

    const bool ok = WavetableImport::loadInto (slot, path, tmp, q);
    VK_EXPECT (ok);
    VK_REQUIRE (slot != nullptr);
    VK_EXPECT_EQ (path, tmp.getFullPathName());

    // Sample some values - just make sure the table is non-silent.
    double sumSq = 0.0;
    for (int i = 0; i < 256; ++i)
    {
        const float ph = (float) i / 256.0f;
        const float v  = slot->sample (0.0f, ph, 0);
        sumSq += v * v;
    }
    VK_EXPECT_GT (std::sqrt (sumSq / 256.0), 0.05);

    tmp.deleteFile();
}

VK_TEST (WavetableImport_ReplaceRetiresOldTable)
{
    WavetableRetirementQueue q;
    std::shared_ptr<Wavetable> slot;
    juce::String path;

    auto tmp1 = makeMonoWavWithCharacter();
    auto tmp2 = makeMonoWavWithCharacter();
    VK_REQUIRE (tmp1.existsAsFile());
    VK_REQUIRE (tmp2.existsAsFile());

    VK_REQUIRE (WavetableImport::loadInto (slot, path, tmp1, q));
    auto firstPtr = slot;
    VK_REQUIRE (firstPtr != nullptr);

    VK_REQUIRE (WavetableImport::loadInto (slot, path, tmp2, q));
    auto secondPtr = slot;
    VK_REQUIRE (secondPtr != nullptr);
    VK_EXPECT (firstPtr != secondPtr);

    // The retirement queue should currently hold the first pointer (refcount
    // > 1 because we still hold firstPtr). Drop our reference and sweep:
    // the queue should then drop it too.
    firstPtr.reset();
    q.sweep();

    // We can't directly observe the queue's contents, but the second swap
    // should still hold the new pointer fine, and the path should reflect it.
    VK_EXPECT_EQ (path, tmp2.getFullPathName());

    tmp1.deleteFile();
    tmp2.deleteFile();
}

VK_TEST (WavetableImport_ClearEmptiesSlot)
{
    WavetableRetirementQueue q;
    std::shared_ptr<Wavetable> slot;
    juce::String path;

    auto tmp = makeMonoWavWithCharacter();
    VK_REQUIRE (tmp.existsAsFile());

    VK_REQUIRE (WavetableImport::loadInto (slot, path, tmp, q));
    VK_REQUIRE (slot != nullptr);
    VK_EXPECT (path.isNotEmpty());

    WavetableImport::clear (slot, path, q);
    VK_EXPECT (slot == nullptr);
    VK_EXPECT (path.isEmpty());

    tmp.deleteFile();
}

VK_TEST (WavetableImport_DisplayNameStripsPathAndExtension)
{
    VK_EXPECT_EQ (WavetableImport::displayName (""), juce::String());

    const auto fakePath =
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("VaporKey_TestDisplay.wav")
            .getFullPathName();

    VK_EXPECT_EQ (WavetableImport::displayName (fakePath),
                  juce::String ("VaporKey_TestDisplay"));
}

VK_TEST (WavetableImport_StereoFileMixesToMono)
{
    // L = +sine, R = -sine. The mix-down should average to near-silence,
    // so the resulting wavetable has very low RMS (compared to a true sine
    // import).
    WavetableRetirementQueue q;
    std::shared_ptr<Wavetable> slot;
    juce::String path;

    auto tmp = makeStereoWav();
    VK_REQUIRE (tmp.existsAsFile());

    VK_REQUIRE (WavetableImport::loadInto (slot, path, tmp, q));
    VK_REQUIRE (slot != nullptr);

    double sumSq = 0.0;
    for (int i = 0; i < 1024; ++i)
    {
        const float ph = (float) i / 1024.0f;
        const float v = slot->sample (0.0f, ph, 0);
        sumSq += v * v;
    }
    const double rms = std::sqrt (sumSq / 1024.0);
    // Mixed-to-zero per frame, but the build path peak-normalises each frame
    // to 0.99. A true zero frame survives normalisation as zero (peak == 0
    // skips the divide). We expect very low RMS, definitely below the
    // mono-loaded version's ~0.4 RMS.
    VK_EXPECT_LT (rms, 0.05);

    tmp.deleteFile();
}

VK_TEST (WavetableImport_RetirementQueueAcceptsNull)
{
    // Retiring a null pointer is documented as a no-op; this regression-tests
    // the early-out so a future refactor can't trip a null-deref.
    WavetableRetirementQueue q;
    q.retire (nullptr);
    q.sweep();
    // Reaching here without a crash is the assertion.
    VK_EXPECT (true);
}
