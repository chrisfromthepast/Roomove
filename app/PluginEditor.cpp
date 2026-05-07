#include "PluginProcessor.h"
#include "PluginEditor.h"

ArmorAudioEditor::ArmorAudioEditor (ArmorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // -- Intensity Knob --
    armorStrengthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    armorStrengthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(armorStrengthKnob);

    strengthLabel.setText("Armor Strength", juce::dontSendNotification);
    strengthLabel.setJustificationType(juce::Justification::centred);
    strengthLabel.attachToComponent(&armorStrengthKnob, false);
    addAndMakeVisible(strengthLabel);

    // Uncomment and connect to your APVTS once created in the Processor
    // strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "ARMOR_STRENGTH", armorStrengthKnob);

    // -- VU Meter --
    addAndMakeVisible(vuMeter);

    // -- Error Logging --
    errorLog.setMultiLine(true);
    errorLog.setReadOnly(true);
    errorLog.setScrollbarsShown(true);
    errorLog.setCaretVisible(false);
    addAndMakeVisible(errorLog);

    logLabel.setText("Error Log", juce::dontSendNotification);
    logLabel.attachToComponent(&errorLog, false);
    addAndMakeVisible(logLabel);

    // Window size and UI polling rate
    setSize (400, 350);
    startTimerHz(30); 
}

ArmorAudioEditor::~ArmorAudioEditor() {}

void ArmorAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // Draw VU Meter Background & Level
    auto vuBounds = vuMeter.getBounds().toFloat();
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(vuBounds);
    
    g.setColour(juce::Colours::green);
    float fillHeight = vuBounds.getHeight() * currentVuLevel;
    
    // Fills from bottom to top
    g.fillRect(vuBounds.getX(), vuBounds.getBottom() - fillHeight, vuBounds.getWidth(), fillHeight);
}

void ArmorAudioEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    // Split top half for controls, bottom half for logs
    auto topArea = area.removeFromTop(area.getHeight() / 2);
    
    // Knob on left
    armorStrengthKnob.setBounds(topArea.removeFromLeft(topArea.getWidth() / 2).reduced(10, 30));
    
    // VU Meter on right
    auto vuArea = topArea.reduced(20, 30);
    vuArea.setWidth(30);
    vuMeter.setBounds(vuArea);

    // Log at the bottom
    area.removeFromTop(20); // Spacing for the label
    errorLog.setBounds(area);
}

void ArmorAudioEditor::timerCallback()
{
    // 1. Update VU Meter
    // Requires an atomic float in your Processor (e.g., std::atomic<float> currentMaskLevel)
    // currentVuLevel = audioProcessor.currentMaskLevel.load();
    
    // 2. Poll for Errors
    // Requires a thread-safe string queue or simple lock-free flag in your Processor
    /*
    juce::String newError = audioProcessor.popNextErrorMessage();
    if (newError.isNotEmpty()) 
    {
        errorLog.moveCaretToEnd();
        errorLog.insertTextAtCaret(newError + "\n");
    }
    */

    repaint(); // Triggers the paint() method to update the VU meter
}
