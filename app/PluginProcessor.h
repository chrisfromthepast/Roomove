#include <RTNeural/RTNeural.h>

class ArmorAudioProcessor : public juce::AudioProcessor 
{
public:
    // ... standard JUCE methods ...
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

private:
    // Neural Brain
    std::unique_ptr<RTNeural::ModelT<float, 1, 1>> model; // Adjust to your Skip-U-Net size
    
    // Lock-Free FIFO for the Mask
    juce::AbstractFifo fifo { 1024 }; 
    float maskBuffer[1024]; 
    float currentMask = 1.0f; // Default to pass-through

    // Background thread for math
    void runInference();
    juce::AudioBuffer<float> sidechainBuffer;
};
