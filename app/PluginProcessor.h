#pragma once

#include <JuceHeader.h>
#include "../dsp/RoomoveDSP.h"

namespace RoomoveParameterIds
{
    inline constexpr auto armorStrength = "armor_strength";
}

class ArmorAudioProcessor : public juce::AudioProcessor
{
public:
    ArmorAudioProcessor();
    ~ArmorAudioProcessor() override = default;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    std::atomic<float>* armorStrengthValue = nullptr;
    std::vector<RoomoveDspState> dspStates;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArmorAudioProcessor)
};
