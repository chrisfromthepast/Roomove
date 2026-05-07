#include "PluginEditor.h"

void MachinedFaderLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 float sliderPos,
                                                 float minSliderPos,
                                                 float maxSliderPos,
                                                 const juce::Slider::SliderStyle sliderStyle,
                                                 juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, sliderStyle, slider);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle ((float) x + (float) width * 0.45f,
                            (float) y,
                            (float) width * 0.1f,
                            (float) height,
                            2.0f);

    const auto faderWidth = (float) width * 0.8f;
    constexpr auto faderHeight = 60.0f;
    const auto faderX = (float) x + ((float) width - faderWidth) * 0.5f;
    const auto faderY = sliderPos - (faderHeight * 0.5f);

    const juce::Colour gradientStart (0xff323232);
    const juce::Colour gradientEnd (0xff1a1a1a);
    const juce::ColourGradient gradient (gradientStart,
                                         faderX,
                                         faderY,
                                         gradientEnd,
                                         faderX,
                                         faderY + faderHeight,
                                         false);

    g.setGradientFill (gradient);
    g.fillRoundedRectangle (faderX, faderY, faderWidth, faderHeight, 4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    for (int i = 1; i < 6; ++i)
    {
        const auto lineY = faderY + ((float) i * (faderHeight / 6.0f));
        g.drawLine (faderX + 5.0f, lineY, faderX + faderWidth - 5.0f, lineY, 1.5f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    auto font = juce::Font (12.0f, juce::Font::bold);
    font.setTypefaceName (juce::Font::getDefaultMonospacedFontName());
    g.setFont (font);
    g.drawText ("ROOM",
                juce::Rectangle<int> ((int) std::round (faderX),
                                      (int) std::round (faderY + (faderHeight * 0.5f) - 6.0f),
                                      (int) std::round (faderWidth),
                                      12),
                juce::Justification::centred);
}

RoomoveAudioEditor::RoomoveAudioEditor (ArmorAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    setSize (150, 400);

    roomSlider.setSliderStyle (juce::Slider::LinearVertical);
    roomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    roomSlider.setLookAndFeel (&machinedLookAndFeel);
    addAndMakeVisible (roomSlider);

    roomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "armor_strength", roomSlider);
}

RoomoveAudioEditor::~RoomoveAudioEditor()
{
    roomSlider.setLookAndFeel (nullptr);
}

void RoomoveAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222222));
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds(), 2);
}

void RoomoveAudioEditor::resized()
{
    roomSlider.setBounds (getLocalBounds().reduced (20, 40));
}
