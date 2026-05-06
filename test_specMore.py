import os
import torch
import torchaudio
import csv
import random

# --- Configuration ---
DATA_DIR = './paired_room_data'
LABEL_FILE = os.path.join(DATA_DIR, 'labels.csv')
MODEL_PATH = 'spec_subtractor.pth'
OUT_DIR = './test_results_spec_more'
N_FFT = 2048        # Match your new training config
HOP_LENGTH = 512    # Match your new training config
SR = 48000

os.makedirs(OUT_DIR, exist_ok=True)
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'mps' if torch.backends.mps.is_available() else 'cpu')

def run_spec_test():
    # 1. Load Model
    from spectroTrainMore import SpecUNet
    model = SpecUNet().to(DEVICE)
    if os.path.exists(MODEL_PATH):
        model.load_state_dict(torch.load(MODEL_PATH, map_location=DEVICE))
    model.eval()

    # 2. Pick Test Case
    with open(LABEL_FILE, 'r') as f:
        reader = list(csv.DictReader(f))
        test_case = random.choice(reader)
    
    wet_path = os.path.join(DATA_DIR, test_case['wet_file'])
    dry_path = os.path.join(DATA_DIR, test_case['dry_file'])
    
    wet_sig, _ = torchaudio.load(wet_path)
    dry_sig, _ = torchaudio.load(dry_path)
    
    wet_sig = wet_sig.to(DEVICE)

    # 3. Process
    with torch.no_grad():
        # Initialize the Hann Window to stop spectral leakage
        window = torch.hann_window(N_FFT).to(DEVICE)

        # Complex Spectrogram for Phase preservation
        complex_spec = torch.stft(
            wet_sig, 
            n_fft=N_FFT, 
            hop_length=HOP_LENGTH, 
            window=window, 
            return_complex=True
        )
        
        mag = torch.abs(complex_spec)
        phase = torch.angle(complex_spec)

        # Apply Power-law compression (0.3) to match training
        input_mag = mag.unsqueeze(0).pow(0.3) 
        
        # Correct Padding for Conv2d stride alignment
        pad_h = 4 - (input_mag.shape[2] % 4)
        pad_w = 4 - (input_mag.shape[3] % 4)
        input_mag = torch.nn.functional.pad(input_mag, (0, pad_w, 0, pad_h))
        
        # Generate Masked Magnitude
        refined_mag_compressed = model(input_mag)
        
        # Remove padding and reverse the Power-law compression
        refined_mag = refined_mag_compressed[:, :, :-pad_h, :-pad_w].squeeze(0).pow(1/0.3)

        # 4. Reconstruct using Original Phase
        real = refined_mag * torch.cos(phase)
        imag = refined_mag * torch.sin(phase)
        recombined = torch.complex(real, imag)
        
        # Inverse STFT with the same window
        output_audio = torch.istft(
            recombined, 
            n_fft=N_FFT, 
            hop_length=HOP_LENGTH, 
            window=window
        )

    # 5. Save the Trinity for comparison
    torchaudio.save(os.path.join(OUT_DIR, "0_input_wet.wav"), wet_sig.cpu(), SR)
    torchaudio.save(os.path.join(OUT_DIR, "1_output_reconstructed.wav"), output_audio.cpu(), SR)
    torchaudio.save(os.path.join(OUT_DIR, "2_target_dry_ground_truth.wav"), dry_sig, SR)
    
    print(f"Results saved to {OUT_DIR}")
    print(f"Sample: {test_case['wet_file']} | IR: {test_case['ir_source']}")

if __name__ == "__main__":
    run_spec_test()