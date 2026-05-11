#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../dsp/RoomoveDSP.h"

namespace
{
    constexpr auto armorStrengthParameterName = "Armor Strength";
    constexpr auto armorStrengthParameterVersion = 1;
}

ArmorAudioProcessor::ArmorAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::mono(), true)
        .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    armorStrengthValue = apvts.getRawParameterValue (RoomoveParameterIds::armorStrength);
}

juce::AudioProcessorValueTreeState::ParameterLayout ArmorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { RoomoveParameterIds::armorStrength, armorStrengthParameterVersion },
        armorStrengthParameterName,
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        1.0f));

    return { parameters.begin(), parameters.end() };
}

void ArmorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    roomoveDspInit ((float) sampleRate);
}

void ArmorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto armorStrength = armorStrengthValue != nullptr ? armorStrengthValue->load() : 1.0f;
    roomoveDspSetArmorStrength (armorStrength);

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        processRoomoveAudio (channelData, channelData, buffer.getNumSamples());
    }
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
