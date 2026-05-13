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

    // --- 1. THE TRACK (Deep, dark inset) ---
    const float trackWidth = (float) width * 0.15f;
    const float trackX = (float) x + ((float) width - trackWidth) * 0.5f;
    
    // Track shadow/depth
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (trackX, (float) y, trackWidth, (float) height, 3.0f);
    
    // Subtle track highlight to simulate a physical slot
    g.setColour (juce::Colour (0xff333333));
    g.drawRoundedRectangle (trackX, (float) y, trackWidth, (float) height, 3.0f, 1.0f);

    // --- 2. THE CAP DIMENSIONS ---
    const auto faderWidth = (float) width * 0.85f;
    constexpr auto faderHeight = 64.0f;
    const auto faderX = (float) x + ((float) width - faderWidth) * 0.5f;
    const auto unclampedFaderY = sliderPos - (faderHeight * 0.5f);
    const auto faderY = juce::jlimit ((float) y, (float) y + (float) height - faderHeight, unclampedFaderY);

    // --- 3. THE ALUMINUM CAP (Silver/Metallic Gradient) ---
    const juce::Colour gradientStart (0xfff5f5f5); // Bright silver highlight
    const juce::Colour gradientEnd (0xff9a9a9a);   // Darker aluminum base
    const juce::ColourGradient gradient (gradientStart,
                                         faderX,
                                         faderY,
                                         gradientEnd,
                                         faderX,
                                         faderY + faderHeight,
                                         false);

    g.setGradientFill (gradient);
    g.fillRoundedRectangle (faderX, faderY, faderWidth, faderHeight, 4.0f);

    // Cap outer border (defines the edge)
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (faderX, faderY, faderWidth, faderHeight, 4.0f, 1.0f);

    // --- 4. ETCHED GROOVES ---
    for (int i = 1; i < 6; ++i)
    {
        const auto lineY = faderY + ((float) i * (faderHeight / 6.0f));
        
        // Dark inner shadow of the groove
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (faderX + 6.0f, lineY, faderX + faderWidth - 6.0f, lineY, 1.5f);
        
        // Bright lower highlight to create the 3D cut effect
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (faderX + 6.0f, lineY + 1.5f, faderX + faderWidth - 6.0f, lineY + 1.5f, 1.0f);
    }

    // --- 5. DEBOSSED TEXT ---
    constexpr auto textVerticalOffset = 8.0f;
    auto font = juce::Font (12.0f, juce::Font::bold);
    font.setTypefaceName (juce::Font::getDefaultMonospacedFontName());
    g.setFont (font);
    
    juce::Rectangle<int> textBounds ((int) std::round (faderX),
                                     (int) std::round (faderY + (faderHeight * 0.5f) - textVerticalOffset),
                                     (int) std::round (faderWidth),
                                     12);

    // White highlight shifted down 1px (Bottom edge of the engraving)
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawText ("ROOM", textBounds.translated(0, 1), juce::Justification::centred);

    // Pure black text (The deep part of the engraving)
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText ("ROOM", textBounds, juce::Justification::centred);
}

void RoomoveAudioEditor::paint (juce::Graphics& g)
{
    // Matte dark charcoal background
    g.fillAll (juce::Colour (0xff141414));
    
    // High-contrast industrial border
    juce::Rectangle<float> borderBounds = getLocalBounds().toFloat().reduced(1.0f);
    
    // Dark outer edge
    g.setColour (juce::Colours::black);
    g.drawRect (borderBounds, 2.0f);
    
    // Silver inner bezel
    g.setColour (juce::Colour (0xff888888));
    g.drawRect (borderBounds.reduced(2.0f), 1.0f);
}
