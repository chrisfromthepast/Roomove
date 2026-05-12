// dsp/RoomoveDSP.cpp
// TI C6000-safe DSP core with C linkage and no JUCE dependencies.

#include "RoomoveDSP.h"

#include <math.h>

#if defined(__TMS320C6X__)
#include <c6x.h>
#endif

namespace
{
    typedef char AssertUnsignedIntIs32Bits[(sizeof(unsigned int) == 4) ? 1 : -1];

    static const float kDefaultSampleRate = 48000.0f;
    static const float kHopLengthSamples = 512.0f;
    static const float kMaskFloor = 0.02f;
    static const float kMaskCeil = 1.0f;
    static const float kStrengthFloor = 0.0f;
    static const float kStrengthCeil = 1.0f;
    static const float kDefaultMask = 1.0f;
    static const float kDefaultStrength = 1.0f;
    static const float kMaskTauSeconds = 0.008f;
    static const float kEnvelopeReleaseTauSeconds = 0.045f;
    static const float kEnvelopeEpsilon = 1.0e-6f;
    static const int kOverlapScratchSamples = 1024;

    static inline float clampf(float x, float lo, float hi)
    {
        return (x < lo) ? lo : ((x > hi) ? hi : x);
    }

    static inline float sanitizeDenormal(float x)
    {
        return (x > -1.0e-20f && x < 1.0e-20f) ? 0.0f : x;
    }

    static inline unsigned int floatToBits(float x)
    {
        union
        {
            float f;
            unsigned int u;
        } v;
        v.f = x;
        return v.u;
    }

    static inline float bitsToFloat(unsigned int x)
    {
        union
        {
            float f;
            unsigned int u;
        } v;
        v.u = x;
        return v.f;
    }

    static inline float computeMaskSmoothing(float sampleRate)
    {
        const float hopTimeSeconds = kHopLengthSamples / sampleRate;
        const float alpha = hopTimeSeconds / (kMaskTauSeconds + hopTimeSeconds);
        return clampf(alpha, 0.01f, 0.5f);
    }

    static inline float computeEnvelopeRelease(float sampleRate)
    {
        return expf(-1.0f / (kEnvelopeReleaseTauSeconds * sampleRate));
    }

    static inline void atomicStoreU32(volatile unsigned int* location, unsigned int value)
    {
#if defined(__GNUC__) || defined(__clang__)
        (void)__sync_lock_test_and_set(location, value);
        __sync_synchronize();
#else
        *location = value;
#endif
    }

    static inline unsigned int atomicLoadU32(volatile unsigned int* location)
    {
#if defined(__GNUC__) || defined(__clang__)
        __sync_synchronize();
        return __sync_add_and_fetch(location, 0U);
#else
        return *location;
#endif
    }

#if defined(__TMS320C6X__) && !defined(__cplusplus)
    static inline void assumeAligned8(const void* pointer)
    {
        _nassert((((unsigned int) pointer) & 0x7U) == 0U);
    }
#else
    static inline void assumeAligned8(const void* pointer)
    {
        (void) pointer;
    }
#endif

    static RoomoveDspState makeDefaultState()
    {
        RoomoveDspState state;
        state.sampleRate = kDefaultSampleRate;
        state.armorStrengthBits = floatToBits(kDefaultStrength);
        state.targetMaskBits = floatToBits(kDefaultMask);
        state.currentMask = kDefaultMask;
        state.maskSmoothing = computeMaskSmoothing(kDefaultSampleRate);
        state.peakEnvelope = 0.0f;
        state.envelopeRelease = computeEnvelopeRelease(kDefaultSampleRate);
        return state;
    }

    static RoomoveDspState gGlobalState = makeDefaultState();
}

extern "C"
{
    void roomoveDspStateInit(RoomoveDspState* state, float sampleRate)
    {
        if (state == 0)
            return;

        state->sampleRate = (sampleRate > 1000.0f) ? sampleRate : kDefaultSampleRate;
        atomicStoreU32(&state->armorStrengthBits, floatToBits(kDefaultStrength));
        atomicStoreU32(&state->targetMaskBits, floatToBits(kDefaultMask));
        state->currentMask = kDefaultMask;
        state->peakEnvelope = 0.0f;

        state->maskSmoothing = computeMaskSmoothing(state->sampleRate);
        state->envelopeRelease = computeEnvelopeRelease(state->sampleRate);
    }

    void roomoveDspStateSetArmorStrength(RoomoveDspState* state, float armorStrength)
    {
        if (state == 0)
            return;

        const float clamped = clampf(armorStrength, kStrengthFloor, kStrengthCeil);
        atomicStoreU32(&state->armorStrengthBits, floatToBits(clamped));
    }

    void roomoveDspStateSetMask(RoomoveDspState* state, float mask)
    {
        if (state == 0)
            return;

        const float clamped = clampf(mask, kMaskFloor, kMaskCeil);
        atomicStoreU32(&state->targetMaskBits, floatToBits(clamped));
    }

    // No-alias kernel for TI software pipeline optimization.
    void roomoveDspStateProcessAudioNoAlias(RoomoveDspState* state, const float* AAX_RESTRICT inputBuffer, float* AAX_RESTRICT outputBuffer, int numSamples)
    {
        if (state == 0 || inputBuffer == 0 || outputBuffer == 0 || numSamples <= 0)
            return;

        const float armorStrength = bitsToFloat(atomicLoadU32(&state->armorStrengthBits));
        const float targetMaskFromHost = bitsToFloat(atomicLoadU32(&state->targetMaskBits));

#if defined(__TMS320C6X__)
        assumeAligned8(inputBuffer);
        assumeAligned8(outputBuffer);
#pragma MUST_ITERATE(4,,4)
#endif
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = sanitizeDenormal(inputBuffer[i]);
            const float magnitude = fabsf(x);

            if (magnitude >= state->peakEnvelope)
                state->peakEnvelope = magnitude;
            else
                state->peakEnvelope = (state->peakEnvelope * state->envelopeRelease) + (magnitude * (1.0f - state->envelopeRelease));

            float adaptiveMask = kDefaultMask;

            if (state->peakEnvelope > kEnvelopeEpsilon)
            {
                const float ratio = magnitude / (state->peakEnvelope + kEnvelopeEpsilon);
                // sqrt softens the gain curve so decays are attenuated audibly without hard gating.
                adaptiveMask = clampf(sqrtf(ratio), kMaskFloor, kMaskCeil);
            }

            const float targetMask = (targetMaskFromHost < adaptiveMask) ? targetMaskFromHost : adaptiveMask;
            const float delta = targetMask - state->currentMask;
            state->currentMask += delta * state->maskSmoothing;

            const float blendedMask = 1.0f + ((state->currentMask - 1.0f) * armorStrength);
            outputBuffer[i] = sanitizeDenormal(x * blendedMask);
        }
    }

    void roomoveDspStateProcessAudio(RoomoveDspState* state, const float* inputBuffer, float* outputBuffer, int numSamples)
    {
        if (state == 0 || inputBuffer == 0 || outputBuffer == 0 || numSamples <= 0)
            return;

        const float* inputEnd = inputBuffer + numSamples;
        const float* outputAsConst = outputBuffer;
        const float* outputEnd = outputAsConst + numSamples;
        const int overlaps = (inputBuffer < outputEnd) && (outputAsConst < inputEnd);

        // Route disjoint buffers to the restricted kernel and preserve safe behavior otherwise.
        if (!overlaps)
        {
            roomoveDspStateProcessAudioNoAlias(state, inputBuffer, outputBuffer, numSamples);
            return;
        }

        // Partial overlap is handled using a stack scratch buffer to preserve sequential sample order.
        if (inputBuffer != outputBuffer)
        {
            float scratch[kOverlapScratchSamples];
            int processed = 0;
            while (processed < numSamples)
            {
                const int remaining = numSamples - processed;
                const int chunk = (remaining < kOverlapScratchSamples) ? remaining : kOverlapScratchSamples;
                for (int i = 0; i < chunk; ++i)
                    scratch[i] = inputBuffer[processed + i];
                roomoveDspStateProcessAudioNoAlias(state, scratch, outputBuffer + processed, chunk);
                processed += chunk;
            }
            return;
        }

        const float armorStrength = bitsToFloat(atomicLoadU32(&state->armorStrengthBits));
        const float targetMaskFromHost = bitsToFloat(atomicLoadU32(&state->targetMaskBits));

#if defined(__TMS320C6X__)
        assumeAligned8(inputBuffer);
        assumeAligned8(outputBuffer);
#pragma MUST_ITERATE(4,,4)
#endif
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = sanitizeDenormal(inputBuffer[i]);
            const float magnitude = fabsf(x);

            if (magnitude >= state->peakEnvelope)
                state->peakEnvelope = magnitude;
            else
                state->peakEnvelope = (state->peakEnvelope * state->envelopeRelease) + (magnitude * (1.0f - state->envelopeRelease));

            float adaptiveMask = kDefaultMask;

            if (state->peakEnvelope > kEnvelopeEpsilon)
            {
                const float ratio = magnitude / (state->peakEnvelope + kEnvelopeEpsilon);
                adaptiveMask = clampf(sqrtf(ratio), kMaskFloor, kMaskCeil);
            }

            const float targetMask = (targetMaskFromHost < adaptiveMask) ? targetMaskFromHost : adaptiveMask;
            const float delta = targetMask - state->currentMask;
            state->currentMask += delta * state->maskSmoothing;

            const float blendedMask = 1.0f + ((state->currentMask - 1.0f) * armorStrength);
            outputBuffer[i] = sanitizeDenormal(x * blendedMask);
        }
    }

    void roomoveDspInit(float sampleRate)
    {
        roomoveDspStateInit(&gGlobalState, sampleRate);
    }

    void roomoveDspSetArmorStrength(float armorStrength)
    {
        roomoveDspStateSetArmorStrength(&gGlobalState, armorStrength);
    }

    void roomoveDspSetMask(float mask)
    {
        roomoveDspStateSetMask(&gGlobalState, mask);
    }

    void processRoomoveAudio(const float* inputBuffer, float* outputBuffer, int numSamples)
    {
        roomoveDspStateProcessAudio(&gGlobalState, inputBuffer, outputBuffer, numSamples);
    }

    void processRoomoveAudioNoAlias(const float* AAX_RESTRICT inputBuffer, float* AAX_RESTRICT outputBuffer, int numSamples)
    {
        roomoveDspStateProcessAudioNoAlias(&gGlobalState, inputBuffer, outputBuffer, numSamples);
    }
}
