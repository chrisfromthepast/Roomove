#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ArmorAudioEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    ArmorAudioEditor (ArmorAudioProcessor&);
    ~ArmorAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ArmorAudioProcessor& audioProcessor;

    // 1. Intensity Knob (Armor Strength)
    juce::Slider armorStrengthKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;
    juce::Label strengthLabel;

    // 2. VU Meter
    juce::Component vuMeter;
    float currentVuLevel = 0.0f;

    // 3. Error Logging
    juce::TextEditor errorLog;
    juce::Label logLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArmorAudioEditor)
};
