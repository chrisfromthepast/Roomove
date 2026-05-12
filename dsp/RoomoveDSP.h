#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef AAX_RESTRICT
    #if defined(_MSC_VER)
        #define AAX_RESTRICT __restrict
    #elif defined(__GNUC__) || defined(__clang__)
        #define AAX_RESTRICT __restrict__
    #else
        #define AAX_RESTRICT
    #endif
#endif

typedef struct RoomoveDspState
{
    float sampleRate;
    volatile unsigned int armorStrengthBits;
    volatile unsigned int targetMaskBits;
    float currentMask;
    float maskSmoothing;
    float peakEnvelope;
    float envelopeRelease;
} RoomoveDspState;

void roomoveDspStateInit(RoomoveDspState* state, float sampleRate);
void roomoveDspStateSetArmorStrength(RoomoveDspState* state, float armorStrength);
void roomoveDspStateSetMask(RoomoveDspState* state, float mask);
void roomoveDspStateProcessAudio(RoomoveDspState* state, const float* inputBuffer, float* outputBuffer, int numSamples);
void roomoveDspStateProcessAudioNoAlias(RoomoveDspState* state, const float* AAX_RESTRICT inputBuffer, float* AAX_RESTRICT outputBuffer, int numSamples);

void roomoveDspInit(float sampleRate);
void roomoveDspSetArmorStrength(float armorStrength);
void roomoveDspSetMask(float mask);
void processRoomoveAudio(const float* inputBuffer, float* outputBuffer, int numSamples);
void processRoomoveAudioNoAlias(const float* AAX_RESTRICT inputBuffer, float* AAX_RESTRICT outputBuffer, int numSamples);

#ifdef __cplusplus
}
#endif
