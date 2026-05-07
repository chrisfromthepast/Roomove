// dsp/RoomoveDSP.cpp
// This is the raw hardware pass-through for the HDX cards.
// NO JUCE INCLUDES ALLOWED HERE.

extern "C" {
    // A simple pass-through function to prove the TI compiler works
    void processRoomoveAudio(float* inputBuffer, float* outputBuffer, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            outputBuffer[i] = inputBuffer[i]; // Just copy input to output
        }
    }
}
