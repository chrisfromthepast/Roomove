#include "PluginProcessor.h"

RoomoveAudioProcessor::RoomoveAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

RoomoveAudioProcessor::~RoomoveAudioProcessor() {}

const juce::String RoomoveAudioProcessor::getName() const { return JucePlugin_Name; }

bool RoomoveAudioProcessor::acceptsMidi() const  { return false; }
bool RoomoveAudioProcessor::producesMidi() const { return false; }
bool RoomoveAudioProcessor::isMidiEffect() const { return false; }
double RoomoveAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int RoomoveAudioProcessor::getNumPrograms()                                    { return 1; }
int RoomoveAudioProcessor::getCurrentProgram()                                 { return 0; }
void RoomoveAudioProcessor::setCurrentProgram (int)                            {}
const juce::String RoomoveAudioProcessor::getProgramName (int)                 { return {}; }
void RoomoveAudioProcessor::changeProgramName (int, const juce::String&)       {}

void RoomoveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void RoomoveAudioProcessor::releaseResources() {}

void RoomoveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels();
         channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

bool RoomoveAudioProcessor::hasEditor() const { return false; }

juce::AudioProcessorEditor* RoomoveAudioProcessor::createEditor()
{
    return nullptr;
}

void RoomoveAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void RoomoveAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomoveAudioProcessor();
}
