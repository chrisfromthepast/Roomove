// dsp/RoomoveDSP.cpp
// TI C6000-safe DSP core with C linkage and no JUCE dependencies.

#include <stdint.h>

#if defined(__TMS320C6X__)
#include <c6x.h>
#endif

namespace
{
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
    static volatile uint32_t gArmorStrengthBits = 0x3f800000U;
    static volatile uint32_t gTargetMaskBits = 0x3f800000U;
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

    static inline uint32_t floatToBits(float x)
    {
        union
        {
            float f;
            uint32_t u;
        } v;
        v.f = x;
        return v.u;
    }

    static inline float bitsToFloat(uint32_t x)
    {
        union
        {
            float f;
            uint32_t u;
        } v;
        v.u = x;
        return v.f;
    }

    static inline void atomicStoreU32(volatile uint32_t* location, uint32_t value)
    {
#if defined(__GNUC__) || defined(__clang__)
        (void)__sync_lock_test_and_set(location, value);
        __sync_synchronize();
#else
        *location = value;
#endif
    }

    static inline uint32_t atomicLoadU32(volatile uint32_t* location)
    {
#if defined(__GNUC__) || defined(__clang__)
        __sync_synchronize();
        return __sync_add_and_fetch(location, 0U);
#else
        return *location;
#endif
    }
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

#if defined(__TMS320C6X__)
        _nassert(((int)inputBuffer & 0x7) == 0);
        _nassert(((int)outputBuffer & 0x7) == 0);
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
