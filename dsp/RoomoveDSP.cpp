// dsp/RoomoveDSP.cpp
// TI C6000-safe DSP core with C linkage and no JUCE dependencies.

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

    static float gSampleRate = 48000.0f;
    static volatile unsigned int gArmorStrengthBits = 0x3f800000U;
    static volatile unsigned int gTargetMaskBits = 0x3f800000U;
    static float gCurrentMask = 1.0f;
    static float gMaskSmoothing = 0.12f;

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
}

extern "C"
{
    void roomoveDspInit(float sampleRate)
    {
        gSampleRate = (sampleRate > 1000.0f) ? sampleRate : kDefaultSampleRate;
        atomicStoreU32(&gArmorStrengthBits, floatToBits(kDefaultStrength));
        atomicStoreU32(&gTargetMaskBits, floatToBits(kDefaultMask));
        gCurrentMask = kDefaultMask;

        const float hopTimeSeconds = kHopLengthSamples / gSampleRate;
        const float alpha = hopTimeSeconds / (kMaskTauSeconds + hopTimeSeconds);
        gMaskSmoothing = clampf(alpha, 0.01f, 0.5f);
    }

    void roomoveDspSetArmorStrength(float armorStrength)
    {
        const float clamped = clampf(armorStrength, kStrengthFloor, kStrengthCeil);
        atomicStoreU32(&gArmorStrengthBits, floatToBits(clamped));
    }

    void roomoveDspSetMask(float mask)
    {
        const float clamped = clampf(mask, kMaskFloor, kMaskCeil);
        atomicStoreU32(&gTargetMaskBits, floatToBits(clamped));
    }

    void processRoomoveAudio(float* inputBuffer, float* outputBuffer, int numSamples)
    {
        if (inputBuffer == 0 || outputBuffer == 0 || numSamples <= 0)
            return;

        const float armorStrength = bitsToFloat(atomicLoadU32(&gArmorStrengthBits));
        const float targetMask = bitsToFloat(atomicLoadU32(&gTargetMaskBits));

        assumeAligned8(inputBuffer);
        assumeAligned8(outputBuffer);
#if defined(__TMS320C6X__)
#pragma MUST_ITERATE(4,,4)
#endif
        for (int i = 0; i < numSamples; ++i)
        {
            const float delta = targetMask - gCurrentMask;
            gCurrentMask += delta * gMaskSmoothing;

            const float blendedMask = 1.0f + ((gCurrentMask - 1.0f) * armorStrength);
            const float x = sanitizeDenormal(inputBuffer[i]);
            outputBuffer[i] = sanitizeDenormal(x * blendedMask);
        }
    }
}
