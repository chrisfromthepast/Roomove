// tests/DspSeamTests.cpp
// Phase 0: smoke tests that exercise each documented Roomove DSP test seam.
// No JUCE dependency — links only dsp/RoomoveDSP.cpp and standard C++ libraries.
// See docs/dsp-test-seams.md for the full inventory and mapping table.

#include "RoomoveDSP.h"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
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

    // -------------------------------------------------------------------------
    // Seam 1: roomoveDspStateInit
    // Verifies that a freshly initialised state carries the documented defaults.
    // -------------------------------------------------------------------------
    bool testDefaultStateAfterInit()
    {
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);

        if (!assertCondition (floatNear (state.sampleRate, 48000.0f),
                              "default sampleRate should be 48000"))
            return false;

        if (!assertCondition (floatNear (state.currentMask, 1.0f),
                              "default currentMask should be 1.0"))
            return false;

        if (!assertCondition (floatNear (state.peakEnvelope, 0.0f),
                              "default peakEnvelope should be 0.0"))
            return false;

        if (!assertCondition (state.maskSmoothing > 0.0f && state.maskSmoothing <= 0.5f,
                              "maskSmoothing should be in (0, 0.5]"))
            return false;

        if (!assertCondition (state.envelopeRelease > 0.0f && state.envelopeRelease < 1.0f,
                              "envelopeRelease should be in (0, 1)"))
            return false;

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 2: roomoveDspStateSetArmorStrength → processAudio
    // At armorStrength=0: blendedMask=1.0 → output must equal input exactly
    // for non-denormal values (pass-through bypass condition).
    // -------------------------------------------------------------------------
    bool testBypassAtArmorStrengthZero()
    {
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 0.0f);

        const int n = 64;
        float input[64];
        float output[64];
        for (int i = 0; i < n; ++i)
            input[i] = 0.125f * (float) ((i % 8) + 1);  // 0.125 .. 1.0, non-denormal

        roomoveDspStateProcessAudio (&state, input, output, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (output[i], input[i]),
                                  "bypass mismatch at sample " + std::to_string (i)
                                      + ": expected " + std::to_string (input[i])
                                      + " got " + std::to_string (output[i])))
                return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 3: roomoveDspStateSetArmorStrength clamping
    // Values outside [0, 1] must be clamped before being stored.
    // -------------------------------------------------------------------------
    bool testArmorStrengthClamping()
    {
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);

        const int n = 4;
        float input[4]  = { 0.5f, 0.5f, 0.5f, 0.5f };
        float outBelow[4], outAbove[4], outZero[4], outOne[4];

        // armorStrength below floor (0.0) — should clamp to 0.0 → pass-through
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, -99.0f);
        roomoveDspStateProcessAudio (&state, input, outBelow, n);

        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 0.0f);
        roomoveDspStateProcessAudio (&state, input, outZero, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (outBelow[i], outZero[i]),
                                  "clamping below 0: sample " + std::to_string (i)
                                      + " differs from armorStrength=0.0 result"))
                return false;
        }

        // armorStrength above ceiling (1.0) — should clamp to 1.0
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 999.0f);
        roomoveDspStateProcessAudio (&state, input, outAbove, n);

        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateProcessAudio (&state, input, outOne, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (outAbove[i], outOne[i]),
                                  "clamping above 1: sample " + std::to_string (i)
                                      + " differs from armorStrength=1.0 result"))
                return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 4: roomoveDspStateSetMask clamping
    // Mask values are clamped to [kMaskFloor=0.02, kMaskCeil=1.0].
    // -------------------------------------------------------------------------
    bool testMaskClamping()
    {
        const float kMaskFloor = 0.02f;
        const float kMaskCeil  = 1.0f;

        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);

        const int n = 4;
        float input[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
        float outBelowFloor[4], outAtFloor[4], outAboveCeil[4], outAtCeil[4];

        // mask below floor → clamped to kMaskFloor
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateSetMask (&state, -1.0f);
        roomoveDspStateProcessAudio (&state, input, outBelowFloor, n);

        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateSetMask (&state, kMaskFloor);
        roomoveDspStateProcessAudio (&state, input, outAtFloor, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (outBelowFloor[i], outAtFloor[i]),
                                  "mask clamp below floor mismatch at sample " + std::to_string (i)))
                return false;
        }

        // mask above ceiling → clamped to kMaskCeil (1.0)
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateSetMask (&state, 99.0f);
        roomoveDspStateProcessAudio (&state, input, outAboveCeil, n);

        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateSetMask (&state, kMaskCeil);
        roomoveDspStateProcessAudio (&state, input, outAtCeil, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (outAboveCeil[i], outAtCeil[i]),
                                  "mask clamp above ceiling mismatch at sample " + std::to_string (i)))
                return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 5: in-place processing (input pointer == output pointer)
    // roomoveDspStateProcessAudio must produce the same result whether
    // called in-place or with separate buffers of equal content.
    // -------------------------------------------------------------------------
    bool testInPlaceProcessing()
    {
        RoomoveDspState stateA, stateB;
        roomoveDspStateInit (&stateA, 48000.0f);
        roomoveDspStateInit (&stateB, 48000.0f);
        roomoveDspStateSetArmorStrength (&stateA, 1.0f);
        roomoveDspStateSetArmorStrength (&stateB, 1.0f);

        const int n = 64;
        float inputA[64], inputB[64], outputB[64];
        for (int i = 0; i < n; ++i)
            inputA[i] = inputB[i] = 0.3f * (float) ((i % 16) + 1) / 16.0f;

        // In-place: output written back into inputA
        roomoveDspStateProcessAudio (&stateA, inputA, inputA, n);

        // Separate buffers
        roomoveDspStateProcessAudio (&stateB, inputB, outputB, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (inputA[i], outputB[i]),
                                  "in-place vs separate mismatch at sample " + std::to_string (i)))
                return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 6: silence in → silence out
    // A zero-valued input buffer must produce a zero-valued output buffer
    // (denormal guard ensures near-zero values are flushed to exactly 0.0).
    // -------------------------------------------------------------------------
    bool testSilencePassesThroughClean()
    {
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);

        const int n = 512;
        float input[512], output[512];
        for (int i = 0; i < n; ++i)
            input[i] = 0.0f;

        roomoveDspStateProcessAudio (&state, input, output, n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (output[i], 0.0f),
                                  "silence test: non-zero output at sample " + std::to_string (i)
                                      + " = " + std::to_string (output[i])))
                return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 7: null pointer safety
    // All public API functions must return silently when passed a null pointer.
    // -------------------------------------------------------------------------
    bool testNullPointerSafety()
    {
        float buf[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
        RoomoveDspState state;
        roomoveDspStateInit (&state, 48000.0f);

        // These must not crash.
        roomoveDspStateInit (nullptr, 48000.0f);
        roomoveDspStateSetArmorStrength (nullptr, 0.5f);
        roomoveDspStateSetMask (nullptr, 0.5f);
        roomoveDspStateProcessAudio (nullptr, buf, buf, 4);
        roomoveDspStateProcessAudio (&state, nullptr, buf, 4);
        roomoveDspStateProcessAudio (&state, buf, nullptr, 4);
        roomoveDspStateProcessAudio (&state, buf, buf, 0);
        roomoveDspStateProcessAudioNoAlias (nullptr, buf, buf, 4);
        roomoveDspStateProcessAudioNoAlias (&state, nullptr, buf, 4);
        roomoveDspStateProcessAudioNoAlias (&state, buf, nullptr, 4);
        roomoveDspStateProcessAudioNoAlias (&state, buf, buf, 0);

        return assertCondition (true, "null pointer safety");
    }

    // -------------------------------------------------------------------------
    // Seam 8: partial-overlap safety at stress-size buffers
    // The public process dispatcher must preserve sample order for overlapping
    // ranges by chunking through the scratch path.
    // -------------------------------------------------------------------------
    bool testPartialOverlapSafetyAtStressSize()
    {
        RoomoveDspState overlapState, referenceState;
        roomoveDspStateInit (&overlapState, 48000.0f);
        roomoveDspStateInit (&referenceState, 48000.0f);
        roomoveDspStateSetArmorStrength (&overlapState, 1.0f);
        roomoveDspStateSetArmorStrength (&referenceState, 1.0f);

        constexpr int n = 1536; // exercises the overlap chunking path with a large buffer
        constexpr float kOverlapTestAmplitude = 0.75f;
        constexpr float kOverlapTestPhaseStep = 0.01f;
        std::array<float, n + 1> overlapBuffer;
        std::array<float, n> referenceInput;
        std::array<float, n> referenceOutput;
        for (int i = 0; i < n + 1; ++i)
            overlapBuffer[i] = kOverlapTestAmplitude * std::sin (kOverlapTestPhaseStep * (float) i);
        for (int i = 0; i < n; ++i)
            referenceInput[i] = overlapBuffer[i + 1];

        roomoveDspStateProcessAudio (&overlapState, overlapBuffer.data() + 1, overlapBuffer.data(), n);
        roomoveDspStateProcessAudio (&referenceState, referenceInput.data(), referenceOutput.data(), n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (overlapBuffer[i], referenceOutput[i], 1.0e-5f),
                                  "partial-overlap mismatch at sample " + std::to_string (i)
                                      + ": overlap=" + std::to_string (overlapBuffer[i])
                                      + " reference=" + std::to_string (referenceOutput[i])))
                return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 9: signal stability invariants
    // Processed samples and adaptive state fields should remain finite and
    // bounded across mixed-amplitude input.
    // -------------------------------------------------------------------------
    bool testSignalStabilityInvariants()
    {
        RoomoveDspState state;
        constexpr float kMaskFloor = 0.02f;
        roomoveDspStateInit (&state, 48000.0f);
        roomoveDspStateSetArmorStrength (&state, 1.0f);
        roomoveDspStateSetMask (&state, kMaskFloor);

        constexpr int n = 2048;
        std::array<float, n> input;
        std::array<float, n> output;
        for (int i = 0; i < n; ++i)
        {
            const int phase = i % 8;
            if (phase == 0)
                input[i] = 0.0f;
            else if (phase == 1)
                input[i] = 1.0e-12f;
            else if (phase == 2)
                input[i] = -1.0e-12f;
            else if (phase == 3)
                input[i] = 0.75f;
            else if (phase == 4)
                input[i] = -0.75f;
            else if (phase == 5)
                input[i] = 1000.0f;
            else if (phase == 6)
                input[i] = -1000.0f;
            else
                input[i] = 0.25f * std::sin (0.015f * (float) i);
        }

        roomoveDspStateProcessAudio (&state, input.data(), output.data(), n);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (std::isfinite (output[i]),
                                  "non-finite output at sample " + std::to_string (i)))
                return false;
        }

        if (!assertCondition (std::isfinite (state.currentMask) && state.currentMask >= kMaskFloor && state.currentMask <= 1.0f,
                              "currentMask must remain finite and in [0.02, 1.0], got "
                                  + std::to_string (state.currentMask)))
            return false;

        if (!assertCondition (std::isfinite (state.peakEnvelope) && state.peakEnvelope >= 0.0f,
                              "peakEnvelope must remain finite and non-negative, got "
                                  + std::to_string (state.peakEnvelope)))
            return false;

        return true;
    }

    // -------------------------------------------------------------------------
    // Seam 10: low-buffer stress determinism
    // Processing the same stream one sample at a time must match full-block
    // processing when sample order is identical.
    // -------------------------------------------------------------------------
    bool testSingleSampleBlockDeterminism()
    {
        RoomoveDspState fullBlockState, singleSampleState;
        roomoveDspStateInit (&fullBlockState, 48000.0f);
        roomoveDspStateInit (&singleSampleState, 48000.0f);
        roomoveDspStateSetArmorStrength (&fullBlockState, 1.0f);
        roomoveDspStateSetArmorStrength (&singleSampleState, 1.0f);

        constexpr int n = 257;
        constexpr float kPrimaryAmplitude = 0.4f;
        constexpr float kPrimaryPhaseStep = 0.07f;
        constexpr float kSecondaryAmplitude = 0.1f;
        constexpr float kSecondaryPhaseStep = 0.11f;
        std::array<float, n> input;
        std::array<float, n> fullBlockOutput;
        std::array<float, n> singleSampleOutput;
        for (int i = 0; i < n; ++i)
            input[i] = (kPrimaryAmplitude * std::sin (kPrimaryPhaseStep * (float) i))
                       + (kSecondaryAmplitude * std::cos (kSecondaryPhaseStep * (float) i));

        roomoveDspStateProcessAudio (&fullBlockState, input.data(), fullBlockOutput.data(), n);

        for (int i = 0; i < n; ++i)
            roomoveDspStateProcessAudio (&singleSampleState, input.data() + i, singleSampleOutput.data() + i, 1);

        for (int i = 0; i < n; ++i)
        {
            if (!assertCondition (floatNear (singleSampleOutput[i], fullBlockOutput[i], 1.0e-6f),
                                  "single-sample determinism mismatch at sample " + std::to_string (i)
                                      + ": single=" + std::to_string (singleSampleOutput[i])
                                      + " full=" + std::to_string (fullBlockOutput[i])
                                      + " abs_delta="
                                      + std::to_string (std::fabs (singleSampleOutput[i] - fullBlockOutput[i]))))
                return false;
        }

        return true;
    }
}

int main()
{
    bool ok = true;
    ok &= testDefaultStateAfterInit();
    ok &= testBypassAtArmorStrengthZero();
    ok &= testArmorStrengthClamping();
    ok &= testMaskClamping();
    ok &= testInPlaceProcessing();
    ok &= testSilencePassesThroughClean();
    ok &= testNullPointerSafety();
    ok &= testPartialOverlapSafetyAtStressSize();
    ok &= testSignalStabilityInvariants();
    ok &= testSingleSampleBlockDeterminism();

    if (!ok)
        return 1;

    std::cout << "PASS: all Roomove DSP seam tests succeeded." << std::endl;
    return 0;
}
