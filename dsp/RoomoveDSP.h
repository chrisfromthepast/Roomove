#pragma once

#ifdef __cplusplus
extern "C"
{
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
void roomoveDspStateProcessAudio(RoomoveDspState* state, float* inputBuffer, float* outputBuffer, int numSamples);

void roomoveDspInit(float sampleRate);
void roomoveDspSetArmorStrength(float armorStrength);
void roomoveDspSetMask(float mask);
void processRoomoveAudio(float* inputBuffer, float* outputBuffer, int numSamples);

#ifdef __cplusplus
}
#endif
