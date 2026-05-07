#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class RoomoveAudioEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    RoomoveAudioEditor (ArmorAudioProcessor&);
    ~RoomoveAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    class MachinedFaderLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider& slider) override
        {
            juce::ignoreUnused (minSliderPos, maxSliderPos, slider);

            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (x + width * 0.45f, y, width * 0.1f, height, 2.0f);

            auto faderWidth = width * 0.8f;
            auto faderHeight = 60.0f;
            auto faderX = x + (width - faderWidth) * 0.5f;
            auto faderY = sliderPos - (faderHeight * 0.5f);

            juce::ColourGradient grad (juce::Colour (0xff323232),
                                       faderX,
                                       faderY,
                                       juce::Colour (0xff1a1a1a),
                                       faderX,
                                       faderY + faderHeight,
                                       false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (faderX, faderY, faderWidth, faderHeight, 4.0f);

            g.setColour (juce::Colours::black.withAlpha (0.3f));
            for (int i = 1; i < 6; ++i)
            {
                const auto lineY = faderY + (i * (faderHeight / 6.0f));
                g.drawLine (faderX + 5, lineY, faderX + faderWidth - 5, lineY, 1.5f);
            }

            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (juce::Font ("Consolas", 12.0f, juce::Font::bold));
            g.drawText ("ROOM", faderX, faderY + (faderHeight * 0.5f) - 6, faderWidth, 12, juce::Justification::centred);
        }
    };

private:
    ArmorAudioProcessor& audioProcessor;
    MachinedFaderLookAndFeel machinedFaderLookAndFeel;

    juce::Slider armorStrengthFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;
    juce::Label strengthLabel;

    // 2. VU Meter
    juce::Component vuMeter;
    float currentVuLevel = 0.0f;

    // 3. Error Logging
    juce::TextEditor errorLog;
    juce::Label logLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomoveAudioEditor)
};
