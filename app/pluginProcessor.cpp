void ArmorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 1. Load Weights from binary data (Embedded .onnx)
    auto modelData = BinaryData::subtractor_onnx;
    auto modelSize = BinaryData::subtractor_onnxSize;
    
    // RTNeural loading logic (simplified)
    std::stringstream ss;
    ss.write(modelData, modelSize);
    model = RTNeural::json_parser::parseJson<float>(ss);
    model->reset();

    // 2. Pre-allocate EVERYTHING (Essential for S6L stability)
    sidechainBuffer.setSize(1, 512); 
    fifo.reset();
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

        // 2. Apply Mask (Zero Latency path)
        channelData[i] *= currentMask;
    }

    // 3. Trigger background inference (Implementation depends on your threading choice)
    // For a quick test, you can trigger a high-priority background job here
}
