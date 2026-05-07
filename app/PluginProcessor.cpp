#include "PluginProcessor.h"

void ArmorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 1. Pre-allocate EVERYTHING (Safe and required for DSP)
    sidechainBuffer.setSize(1, 512); 
    fifo.reset();

    // 2. Hide the native-only model setup from the TI compiler
#if ROOMOVE_HAS_RTNEURAL
    model.reset();
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
#if ROOMOVE_HAS_RTNEURAL
    // Trigger background inference here when the RTNeural path is available
#endif
}




const juce::String ArmorAudioProcessor::getName() const { return JucePlugin_Name; }

bool ArmorAudioProcessor::acceptsMidi() const  { return false; }
bool ArmorAudioProcessor::producesMidi() const { return false; }
bool ArmorAudioProcessor::isMidiEffect() const { return false; }
double ArmorAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int ArmorAudioProcessor::getNumPrograms()                                    { return 1; }
int ArmorAudioProcessor::getCurrentProgram()                                 { return 0; }
void ArmorAudioProcessor::setCurrentProgram (int)                            {}
const juce::String ArmorAudioProcessor::getProgramName (int)                 { return {}; }
void ArmorAudioProcessor::changeProgramName (int, const juce::String&)       {}

void ArmorAudioProcessor::releaseResources() {}

bool ArmorAudioProcessor::hasEditor() const { return false; }

juce::AudioProcessorEditor* ArmorAudioProcessor::createEditor()
{
    return nullptr;
}

void ArmorAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void ArmorAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArmorAudioProcessor();
}
