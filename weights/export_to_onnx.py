import torch
import os
from training.spectroTrainMore import SpecUNet

# --- Configuration ---
MODEL_PATH = 'spec_subtractor.pth'
ONNX_OUTPUT = 'subtractor.onnx'

# Force CPU for export. Compiling the ONNX graph on CPU ensures maximum 
# compatibility when loading it into C++ inference engines later.
DEVICE = torch.device('cpu') 

def export_model():
    print(f"--- Booting ONNX Exporter ---")
    
    # 1. Initialize the model and load your 50-epoch weights
    model = SpecUNet().to(DEVICE)
    
    if not os.path.exists(MODEL_PATH):
        print(f"[!] Error: Cannot find {MODEL_PATH}. Check your directory.")
        return

    model.load_state_dict(torch.load(MODEL_PATH, map_location=DEVICE))
    
    # VERY IMPORTANT: Set to eval mode. This locks layers like Dropout 
    # or BatchNorm so they don't randomly alter the signal during C++ inference.
    model.eval()

    # 2. Define the Dummy Input
    # C++ needs to know the exact tensor shape to allocate memory.
    # Format: [Batch, Channels, Freq_Bins, Time_Frames]
    # At N_FFT=2048, we have 1025 frequency bins.
    # However, your UNet requires dimensions divisible by 4 to survive the stride downsampling.
    # 1025 padded to the nearest multiple of 4 is 1028.
    # We will use 96 for the time frames (approx 1 second of audio at 48k/512 hop).
    
    dummy_input = torch.randn(1, 1, 1028, 96, device=DEVICE)

    print(f"-> Exporting model to {ONNX_OUTPUT}...")

    # 3. The Export Command
    torch.onnx.export(
        model,                       
        dummy_input,                 
        ONNX_OUTPUT,                 
        export_params=True,          # Store the trained weights inside the file
        opset_version=12,            # Opset 12 is highly stable for RTNeural/JUCE
        do_constant_folding=True,    # Optimizes the math graph for faster inference
        input_names=['input'],       # The C++ buffer hook
        output_names=['output'],     # The C++ mask retrieval hook
        
        # Dynamic axes allow your JUCE plugin to feed it buffers of varying time lengths
        dynamic_axes={
            'input': {3: 'time_frames'}, 
            'output': {3: 'time_frames'}
        }
    )
    
    print(f"✅ Success! {ONNX_OUTPUT} is ready for the audio thread.")

if __name__ == "__main__":
    export_model()