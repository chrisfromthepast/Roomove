// tests/ProcessorTests.cpp
// Phase 3: processor correctness and state round-trip tests.
//
// Tests are organised into two complementary layers:
//
//   (a) Source-inspection tests — parse PluginProcessor.cpp / PluginProcessor.h
//       to confirm that parameter declarations, state serialization, and
//       initialisation patterns match the documented contracts.  These tests
//       link only juce_core and are deterministic in CI without a display or
//       audio driver.
//
//   (b) DSP-layer behavioural tests — exercise the RoomoveDspState C API
//       directly to verify neutral/bypass signal transparency at multiple
//       sample rates and to validate that the init → process → re-init cycle
//       (which mirrors a preset-load followed by prepareToPlay) fully restores
//       a clean initial state.
//
// Inspiration: FleetwoodAir Tests/ProcessorTests.cpp, Tests/FleetwoodAirTests.cpp
// See also:    docs/dsp-test-seams.md §3 (parameters), §4 (bypass), §5 (state)

#include <juce_core/juce_core.h>
#include "RoomoveDSP.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    // =========================================================================
    // Shared utilities
    // =========================================================================

    static bool assertCondition (bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << std::endl;
            return false;
        }
        return true;
    }

    static bool floatNear (float a, float b, float tolerance = 1.0e-6f)
    {
        return std::fabs (a - b) <= tolerance;
    }

    static bool readTextFile (const std::string& path, std::string& content)
    {
        std::ifstream file (path.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
            return false;
        content.assign ((std::istreambuf_iterator<char> (file)),
                        std::istreambuf_iterator<char>());
        return true;
    }

    static std::size_t findMatchingBrace (const std::string& text, std::size_t openBrace)
    {
        int depth = 0;
        for (std::size_t i = openBrace; i < text.size(); ++i)
        {
            if (text[i] == '{')
                ++depth;
            else if (text[i] == '}')
            {
                if (--depth == 0)
                    return i;
            }
        }
        return std::string::npos;
    }

    static bool extractFunctionBody (const std::string& text,
                                     const std::string& token,
                                     std::string& body)
    {
        const std::size_t tokenPos = text.find (token);
        if (tokenPos == std::string::npos)
            return false;
        const std::size_t openBrace = text.find ('{', tokenPos);
        if (openBrace == std::string::npos)
            return false;
        const std::size_t closeBrace = findMatchingBrace (text, openBrace);
        if (closeBrace == std::string::npos)
            return false;
        body = text.substr (openBrace + 1, closeBrace - openBrace - 1);
        return true;
    }

    // =========================================================================
    // (a) Source-inspection tests
    // =========================================================================

    // -------------------------------------------------------------------------
    // Test 1: Parameter declaration contracts
    // Confirms that createParameterLayout declares armor_strength with the
    // documented ID, display name, default value (1.0), and range [0, 1, 0.01].
    // See docs/dsp-test-seams.md §3 for the authoritative contract.
    // -------------------------------------------------------------------------
    bool testParameterDeclarationContracts()
    {
        const std::string headerPath =
            std::string (ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.h";
        const std::string sourcePath =
            std::string (ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";

        std::string header, source;
        if (!readTextFile (headerPath, header))
            return assertCondition (false, "Unable to open PluginProcessor.h");
        if (!readTextFile (sourcePath, source))
            return assertCondition (false, "Unable to open PluginProcessor.cpp");

        bool ok = true;

        // Parameter ID string "armor_strength" must appear in the header namespace
        ok = assertCondition (header.find ("\"armor_strength\"") != std::string::npos,
                              "Parameter ID \"armor_strength\" not found in PluginProcessor.h")
             && ok;

        // Display name "Armor Strength" must be declared in the implementation file
        ok = assertCondition (source.find ("\"Armor Strength\"") != std::string::npos,
                              "Display name \"Armor Strength\" not found in PluginProcessor.cpp")
             && ok;

        // Extract createParameterLayout body for range / default checks
        std::string layoutBody;
        if (!extractFunctionBody (source, "ArmorAudioProcessor::createParameterLayout", layoutBody))
            return assertCondition (false,
                                   "createParameterLayout not found in PluginProcessor.cpp");

        // AudioParameterFloat must be used for the parameter type
        ok = assertCondition (layoutBody.find ("AudioParameterFloat") != std::string::npos,
                              "AudioParameterFloat not used in createParameterLayout")
             && ok;

        // Range minimum 0.0f
        ok = assertCondition (layoutBody.find ("0.0f") != std::string::npos,
                              "Range minimum 0.0f not found in createParameterLayout")
             && ok;

        // Range maximum / default 1.0f
        ok = assertCondition (layoutBody.find ("1.0f") != std::string::npos,
                              "Range maximum / default 1.0f not found in createParameterLayout")
             && ok;

        // Range step 0.01f
        ok = assertCondition (layoutBody.find ("0.01f") != std::string::npos,
                              "Range step 0.01f not found in createParameterLayout")
             && ok;

        return ok;
    }

    // -------------------------------------------------------------------------
    // Test 2: State serialization implementation
    // Confirms that getStateInformation and setStateInformation implement the
    // documented APVTS binary-XML round-trip pattern described in docs §5:
    //   save:    apvts.copyState() → createXml() → copyXmlToBinary
    //   restore: getXmlFromBinary → hasTagName guard → apvts.replaceState
    // -------------------------------------------------------------------------
    bool testStateSerializationImplementation()
    {
        const std::string sourcePath =
            std::string (ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::string source;
        if (!readTextFile (sourcePath, source))
            return assertCondition (false, "Unable to open PluginProcessor.cpp");

        std::string getBody, setBody;
        if (!extractFunctionBody (source,
                                  "ArmorAudioProcessor::getStateInformation",
                                  getBody))
            return assertCondition (false,
                                   "getStateInformation not found in PluginProcessor.cpp");
        if (!extractFunctionBody (source,
                                  "ArmorAudioProcessor::setStateInformation",
                                  setBody))
            return assertCondition (false,
                                   "setStateInformation not found in PluginProcessor.cpp");

        bool ok = true;

        // getStateInformation: must copy APVTS state and write binary XML
        ok = assertCondition (getBody.find ("apvts.copyState") != std::string::npos,
                              "getStateInformation must call apvts.copyState()")
             && ok;
        ok = assertCondition (getBody.find ("copyXmlToBinary") != std::string::npos,
                              "getStateInformation must call copyXmlToBinary")
             && ok;

        // setStateInformation: must deserialise and replace the APVTS state,
        // with a root-tag safety check to reject foreign state blobs
        ok = assertCondition (setBody.find ("getXmlFromBinary") != std::string::npos,
                              "setStateInformation must call getXmlFromBinary")
             && ok;
        ok = assertCondition (setBody.find ("apvts.replaceState") != std::string::npos,
                              "setStateInformation must call apvts.replaceState")
             && ok;
        ok = assertCondition (setBody.find ("hasTagName") != std::string::npos,
                              "setStateInformation must guard with hasTagName to verify root tag")
             && ok;

        return ok;
    }

    // -------------------------------------------------------------------------
    // Test 3: prepareToPlay initialises all DSP states
    // Confirms that prepareToPlay iterates over dspStates and calls
    // roomoveDspStateInit on every channel, so that a preset load followed by
    // prepareToPlay always starts the DSP from a clean, deterministic state.
    // -------------------------------------------------------------------------
    bool testPrepareToPlayInitializesAllStates()
    {
        const std::string sourcePath =
            std::string (ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::string source;
        if (!readTextFile (sourcePath, source))
            return assertCondition (false, "Unable to open PluginProcessor.cpp");

        std::string prepareBody;
        if (!extractFunctionBody (source,
                                  "ArmorAudioProcessor::prepareToPlay",
                                  prepareBody))
            return assertCondition (false,
                                   "prepareToPlay not found in PluginProcessor.cpp");

        bool ok = true;

        // Must call roomoveDspStateInit so each channel is fully reset
        ok = assertCondition (prepareBody.find ("roomoveDspStateInit") != std::string::npos,
                              "prepareToPlay must call roomoveDspStateInit for each state")
             && ok;

        // Must iterate over the dspStates collection (range-for or index loop)
        ok = assertCondition (prepareBody.find ("dspStates") != std::string::npos,
                              "prepareToPlay must iterate over dspStates")
             && ok;

        return ok;
    }

    // -------------------------------------------------------------------------
    // Test 4: Single-program implementation (no multi-preset support)
    // Roomove intentionally exposes exactly one program.  getNumPrograms must
    // return 1 so hosts know there are no bankable presets to manage.
    // -------------------------------------------------------------------------
    bool testSingleProgramImplementation()
    {
        const std::string sourcePath =
            std::string (ROOMOVE_REPO_ROOT) + "/app/PluginProcessor.cpp";
        std::string source;
        if (!readTextFile (sourcePath, source))
            return assertCondition (false, "Unable to open PluginProcessor.cpp");

        std::string numProgramsBody;
        if (!extractFunctionBody (source,
                                  "ArmorAudioProcessor::getNumPrograms",
                                  numProgramsBody))
            return assertCondition (false,
                                   "getNumPrograms not found in PluginProcessor.cpp");

        return assertCondition (
            numProgramsBody.find ("return 1") != std::string::npos,
            "getNumPrograms must return 1 (single-program — no multi-preset support)");
    }

    // =========================================================================
    // (b) DSP-layer behavioural tests
    // =========================================================================

    // -------------------------------------------------------------------------
    // Test 5: Bypass transparency at multiple sample rates
    // At armorStrength = 0.0 the blended mask collapses to 1.0 and output must
    // equal input for any documented host sample rate.
    // Formula (docs §4): blendedMask = 1.0 + (currentMask - 1.0) * 0.0 = 1.0
    // -------------------------------------------------------------------------
    bool testBypassTransparencyAtMultipleSampleRates()
    {
        static const float kSampleRates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
        static const int   kNumRates       = 4;

        const int n = 64;
        float input[64];
        for (int i = 0; i < n; ++i)
            input[i] = 0.125f * (float) ((i % 8) + 1);  // 0.125 .. 1.0, non-denormal

        for (int r = 0; r < kNumRates; ++r)
        {
            const float sr = kSampleRates[r];
            float output[64];

            RoomoveDspState state;
            roomoveDspStateInit (&state, sr);
            roomoveDspStateSetArmorStrength (&state, 0.0f);
            roomoveDspStateProcessAudio (&state, input, output, n);

            for (int i = 0; i < n; ++i)
            {
                if (!assertCondition (floatNear (output[i], input[i]),
                                      "bypass mismatch at "
                                          + std::to_string ((int) sr)
                                          + " Hz, sample " + std::to_string (i)
                                          + ": expected " + std::to_string (input[i])
                                          + " got " + std::to_string (output[i])))
                    return false;
            }
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Test 6: State init–process–re-init round-trip (simulates preset load)
    // Runs the DSP long enough to evolve peakEnvelope and currentMask away from
    // their defaults, then calls roomoveDspStateInit again (the equivalent of
    // the host calling prepareToPlay after setStateInformation).  All fields
    // must return to their documented post-init defaults.
    // -------------------------------------------------------------------------
    bool testStateInitResetCycle()
    {
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);

        // Process 1024 samples to evolve peakEnvelope and currentMask
        const int n = 1024;
        float input[1024];
        float output[1024];
        for (int i = 0; i < n; ++i)
            input[i] = 0.8f * (float) ((i % 16) + 1) / 16.0f;
        roomoveDspStateProcessAudio (&state, input, output, n);

        // Simulate preset load → host calls prepareToPlay → roomoveDspStateInit
        roomoveDspStateInit (&state, 48000.0f);

        bool ok = true;
        ok = assertCondition (floatNear (state.sampleRate, 48000.0f),
                              "after reset, sampleRate must be 48000")
             && ok;
        ok = assertCondition (floatNear (state.currentMask, 1.0f),
                              "after reset, currentMask must be restored to 1.0")
             && ok;
        ok = assertCondition (floatNear (state.peakEnvelope, 0.0f),
                              "after reset, peakEnvelope must be restored to 0.0")
             && ok;
        ok = assertCondition (state.maskSmoothing > 0.0f && state.maskSmoothing <= 0.5f,
                              "after reset, maskSmoothing must be in (0, 0.5]")
             && ok;
        ok = assertCondition (state.envelopeRelease > 0.0f && state.envelopeRelease < 1.0f,
                              "after reset, envelopeRelease must be in (0, 1)")
             && ok;
        return ok;
    }

    // -------------------------------------------------------------------------
    // Test 7: Bypass is signal-transparent after state reset
    // After dirting the DSP state and then re-initialising it, setting
    // armorStrength = 0 must again produce exact pass-through.  This confirms
    // that the reset is total and leaves no stale envelope or mask state that
    // could corrupt the bypass path.
    // -------------------------------------------------------------------------
    bool testBypassAfterStateReset()
    {
        RoomoveDspState state;

        // Dirty the state
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        const int nDirty = 512;
        float dirtyInput[512], dirtyOutput[512];
        for (int i = 0; i < nDirty; ++i)
            dirtyInput[i] = 0.5f;
        roomoveDspStateProcessAudio (&state, dirtyInput, dirtyOutput, nDirty);

        // Re-init (simulates prepareToPlay after setStateInformation) then bypass
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 0.0f);

        const int n = 64;
        float bypassInput[64], bypassOutput[64];
        for (int i = 0; i < n; ++i)
            bypassInput[i] = 0.125f * (float) ((i % 8) + 1);
        roomoveDspStateProcessAudio (&state, bypassInput, bypassOutput, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (bypassOutput[i], bypassInput[i]),
                                  "bypass after reset: mismatch at sample "
                                      + std::to_string (i)
                                      + ": expected " + std::to_string (bypassInput[i])
                                      + " got " + std::to_string (bypassOutput[i])))
                return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Test 8: Block size variation
    // The DSP kernel is sample-sequential with no block-level hidden state, so
    // splitting the same signal into different block sizes must produce
    // bit-identical output.  This verifies that no block-aligned artefacts or
    // off-by-one errors exist in the state update logic.
    // Tested block sizes: 1, 16, 256 (compared against a 512-sample reference).
    // -------------------------------------------------------------------------
    bool testBlockSizeVariation()
    {
        static const int kTotalSamples = 512;
        float referenceInput[512];
        for (int i = 0; i < kTotalSamples; ++i)
            referenceInput[i] = 0.4f * (float) ((i % 32) + 1) / 32.0f;

        // Reference: process all 512 samples in one call
        float referenceOutput[512];
        {
            RoomoveDspState refState;
            roomoveDspStateInit (&refState, 48000.0f);
            roomoveDspStateSetArmorStrength (&refState, 1.0f);
            roomoveDspStateProcessAudio (&refState, referenceInput, referenceOutput, kTotalSamples);
        }

        static const int kBlockSizes[] = { 1, 16, 256 };
        static const int kNumBlockSizes = 3;

        for (int b = 0; b < kNumBlockSizes; ++b)
        {
            const int blockSize = kBlockSizes[b];
            float output[512];

            RoomoveDspState state;
            roomoveDspStateInit (&state, 48000.0f);
            roomoveDspStateSetArmorStrength (&state, 1.0f);

            for (int offset = 0; offset < kTotalSamples; offset += blockSize)
                roomoveDspStateProcessAudio (&state,
                                             referenceInput + offset,
                                             output + offset,
                                             blockSize);

            for (int i = 0; i < kTotalSamples; ++i)
            {
                if (!assertCondition (floatNear (output[i], referenceOutput[i]),
                                      "block size " + std::to_string (blockSize)
                                          + " mismatch at sample " + std::to_string (i)
                                          + ": expected " + std::to_string (referenceOutput[i])
                                          + " got " + std::to_string (output[i])))
                    return false;
            }
        }
        return true;
    }

} // anonymous namespace

int main()
{
    bool ok = true;

    // (a) Source-inspection tests
    ok &= testParameterDeclarationContracts();
    ok &= testStateSerializationImplementation();
    ok &= testPrepareToPlayInitializesAllStates();
    ok &= testSingleProgramImplementation();

    // (b) DSP-layer behavioural tests
    ok &= testBypassTransparencyAtMultipleSampleRates();
    ok &= testStateInitResetCycle();
    ok &= testBypassAfterStateReset();
    ok &= testBlockSizeVariation();

    if (!ok)
        return 1;

    std::cout << "PASS: all Roomove processor correctness tests succeeded." << std::endl;
    return 0;
}
