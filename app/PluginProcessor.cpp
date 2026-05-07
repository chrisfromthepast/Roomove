#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto armorStrengthParameterId = "armor_strength";
    constexpr auto armorStrengthParameterName = "Armor Strength";
    constexpr auto armorStrengthParameterVersion = 1;
}

ArmorAudioProcessor::ArmorAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::mono(), true)
        .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    armorStrengthValue = apvts.getRawParameterValue (armorStrengthParameterId);
}

juce::AudioProcessorValueTreeState::ParameterLayout ArmorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { armorStrengthParameterId, armorStrengthParameterVersion },
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

    // 2. Keep RTNeural setup out of builds that cannot compile it
}

void ArmorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto armorStrength = armorStrengthValue != nullptr ? armorStrengthValue->load() : 1.0f;
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
        // 0.0 bypasses the mask and 1.0 applies the full current mask value.
        const auto blendedMask = 1.0f + ((currentMask - 1.0f) * armorStrength);
        channelData[i] *= blendedMask;
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

bool ArmorAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ArmorAudioProcessor::createEditor()
{
    return new RoomoveAudioEditor (*this);
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
