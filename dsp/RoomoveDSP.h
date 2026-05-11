#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

void roomoveDspInit(float sampleRate);
void roomoveDspSetArmorStrength(float armorStrength);
void roomoveDspSetMask(float mask);
void processRoomoveAudio(float* inputBuffer, float* outputBuffer, int numSamples);

#ifdef __cplusplus
}
#endif
