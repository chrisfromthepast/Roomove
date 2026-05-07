#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MachinedFaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle sliderStyle,
                           juce::Slider& slider) override;
};

class RoomoveAudioEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomoveAudioEditor (ArmorAudioProcessor&);
    ~RoomoveAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MachinedFaderLookAndFeel machinedLookAndFeel;
    juce::Slider roomSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomoveAudioEditor)
};
