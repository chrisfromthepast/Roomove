#include "PluginProcessor.h"

void ArmorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 1. Pre-allocate EVERYTHING (Safe and required for DSP)
    sidechainBuffer.setSize(1, 512); 
    fifo.reset();

    // 2. Hide the weights loading and model reset from the TI compiler
#ifndef __TMS320C6X__
    auto modelData = BinaryData::subtractor_onnx;
    auto modelSize = BinaryData::subtractor_onnxSize;
    
    std::stringstream ss;
    ss.write(modelData, modelSize);
    model = RTNeural::json_parser::parseJson<float>(ss);
    model->reset();
#endif
}

void ArmorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    auto* channelData = buffer.getWritePointer(0);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // 1. Peek at the FIFO for a new mask
        int start1, size1, start2, size2;
        fifo.prepareToRead(1, start1, size1, start2, size2);
        
        if (size1 > 0) {
            currentMask = maskBuffer[start1];
            fifo.finishedRead(1);
        }

        // 2. Apply Mask (Zero Latency path - safe for both Native and DSP)
        channelData[i] *= currentMask;
    }

    // 3. Hide the background thread trigger from the TI compiler
#ifndef __TMS320C6X__
    // Trigger background inference here
#endif
}




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
