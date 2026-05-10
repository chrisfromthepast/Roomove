// dsp/RoomoveDSP.cpp
// TI C6000-safe DSP core with C linkage and no JUCE dependencies.

#include <stdint.h>

#if defined(__TMS320C6X__)
#include <c6x.h>
#endif

namespace
{
    static float gSampleRate = 48000.0f;
    static float gArmorStrength = 1.0f;
    static float gTargetMask = 1.0f;
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
}

extern "C"
{
    void roomoveDspInit(float sampleRate)
    {
        gSampleRate = (sampleRate > 1000.0f) ? sampleRate : 48000.0f;
        gArmorStrength = 1.0f;
        gTargetMask = 1.0f;
        gCurrentMask = 1.0f;

        const float hopTimeSeconds = 512.0f / gSampleRate;
        const float tauSeconds = 0.008f;
        const float alpha = hopTimeSeconds / (tauSeconds + hopTimeSeconds);
        gMaskSmoothing = clampf(alpha, 0.01f, 0.5f);
    }

    void roomoveDspSetArmorStrength(float armorStrength)
    {
        gArmorStrength = clampf(armorStrength, 0.0f, 1.0f);
    }

    void roomoveDspSetMask(float mask)
    {
        gTargetMask = clampf(mask, 0.02f, 1.0f);
    }

    void processRoomoveAudio(float* inputBuffer, float* outputBuffer, int numSamples)
    {
        if (inputBuffer == 0 || outputBuffer == 0 || numSamples <= 0)
            return;

#if defined(__TMS320C6X__)
        _nassert(((int)inputBuffer & 0x7) == 0);
        _nassert(((int)outputBuffer & 0x7) == 0);
#pragma MUST_ITERATE(4,,4)
#endif
        for (int i = 0; i < numSamples; ++i)
        {
            const float delta = gTargetMask - gCurrentMask;
            gCurrentMask += delta * gMaskSmoothing;

            const float blendedMask = 1.0f + ((gCurrentMask - 1.0f) * gArmorStrength);
            const float x = sanitizeDenormal(inputBuffer[i]);
            outputBuffer[i] = sanitizeDenormal(x * blendedMask);
        }
    }
}
