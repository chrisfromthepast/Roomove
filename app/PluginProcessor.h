#pragma once

#include <JuceHeader.h>

#if ! defined(__TMS320C6X__) && __has_include(<RTNeural/RTNeural.h>)
    #include <RTNeural/RTNeural.h>
    #define ROOMOVE_HAS_RTNEURAL 1
#else
    #define ROOMOVE_HAS_RTNEURAL 0
#endif

class ArmorAudioProcessor : public juce::AudioProcessor
{
public:
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
#if ROOMOVE_HAS_RTNEURAL
    std::unique_ptr<RTNeural::ModelT<float, 1, 1>> model;
#endif

    juce::AbstractFifo fifo { 1024 };
    float maskBuffer[1024];
    float currentMask = 1.0f;

#if ROOMOVE_HAS_RTNEURAL
    void runInference();
#endif
    juce::AudioBuffer<float> sidechainBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArmorAudioProcessor)
};
