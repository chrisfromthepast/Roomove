#pragma once
#include <JuceHeader.h>
#ifndef __TMS320C6X__
    #include <RTNeural/RTNeural.h>
#endif

class ArmorAudioProcessor : public juce::AudioProcessor 
{
public:
    // ... standard JUCE methods ...
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

private:
    // 2. Hide the model instance from the TI compiler
#ifndef __TMS320C6X__
    std::unique_ptr<RTNeural::ModelT<float, 1, 1>> model;
#endif
    
    // Lock-Free FIFO for the Mask (Safe for DSP)
    juce::AbstractFifo fifo { 1024 }; 
    float maskBuffer[1024]; 
    float currentMask = 1.0f; // Default to pass-through

    void runInference();
    juce::AudioBuffer<float> sidechainBuffer;
};


private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomoveAudioProcessor)
};
