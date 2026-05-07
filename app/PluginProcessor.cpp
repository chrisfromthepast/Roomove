#include "PluginProcessor.h"

namespace
{
    constexpr auto armorStrengthParameterId = "armor_strength";
    constexpr auto armorStrengthParameterName = "Armor Strength";
}

ArmorAudioProcessor::ArmorAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::mono(), true)
        .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ArmorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { armorStrengthParameterId, 1 },
        armorStrengthParameterName,
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        1.0f));

    return { parameters.begin(), parameters.end() };
}

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

void ArmorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto armorStrength = apvts.getRawParameterValue (armorStrengthParameterId)->load();
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
        const auto blendedMask = 1.0f + ((currentMask - 1.0f) * armorStrength);
        channelData[i] *= blendedMask;
    }

    // 3. Hide the background thread trigger from the TI compiler
#ifndef __TMS320C6X__
    // Trigger background inference here
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

void ArmorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ArmorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArmorAudioProcessor();
}
