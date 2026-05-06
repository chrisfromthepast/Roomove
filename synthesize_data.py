import os
import random
import csv
import torch
import torchaudio
import numpy as np
from scipy.signal import fftconvolve
from tqdm import tqdm

# --- Configuration ---
NUM_SAMPLES = 10000
SR = 48000
# Updated to match the exact paths on Intel101
VOCALS_DIR = './raw_data/vocals/VCTK-Corpus-0.92' 
IR_DIR = './raw_data/impulses/AIR_1_4' 
OUT_DIR = './training_data'
LABEL_FILE = os.path.join(OUT_DIR, 'labels.csv')

os.makedirs(OUT_DIR, exist_ok=True)

def load_audio(path):
    sig, sr = torchaudio.load(path)
    if sig.shape[0] > 1: sig = torch.mean(sig, dim=0, keepdim=True)
    if sr != SR:
        resampler = torchaudio.transforms.Resample(sr, SR)
        sig = resampler(sig)
    return sig.numpy().flatten()


def get_files(path, extensions=('.wav', '.flac')):
    return [os.path.join(dp, f) for dp, dn, filenames in os.walk(path) for f in filenames if f.endswith(extensions)]

def cook():
    print("Locating audio assets (this takes a second for 11GB)...")
    vocals = get_files(VOCALS_DIR)
    irs = get_files(IR_DIR)
    
    if not vocals:
        print(f"Error: No .wav files found in {VOCALS_DIR}. Check extraction.")
        return
    if not irs:
        print(f"Error: No .wav files found in {IR_DIR}. Check extraction.")
        return
        
    print(f"Success! Found {len(vocals)} vocals and {len(irs)} impulses.")
    
    with open(LABEL_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['filename', 'freq_hz'])
        
        for i in tqdm(range(NUM_SAMPLES), desc="Cooking Audio"):
            # 1. Randomly pick assets
            v_path = random.choice(vocals)
            ir_path = random.choice(irs)
            
            # 2. Process
            dry = load_audio(v_path)
            ir = load_audio(ir_path)
            
            # Convolve (Room sound)
            wet = fftconvolve(dry[:SR*3], ir[:SR], mode='full')[:SR*3] # 3 second chunks
            wet /= (np.max(np.abs(wet)) + 1e-6)
            
            # 3. Generate Feedback
            t = np.arange(len(wet)) / SR
            # Logarithmic random freq between 100Hz and 15kHz
            freq = 10**random.uniform(np.log10(100), np.log10(15000))
            
            # Exponential growth
            growth = random.uniform(2.0, 5.0)
            env = np.exp(growth * t)
            env /= (np.max(env) + 1e-6)
            
            osc = np.sin(2 * np.pi * freq * t) * env * random.uniform(0.5, 2.0)
            
            # 4. Mix and Saturate
            output = np.tanh(wet + osc)
            
            # 5. Save
            fname = f"sample_{i:05d}.wav"
           
            torchaudio.save(os.path.join(OUT_DIR, fname), torch.tensor(output, dtype=torch.float32).unsqueeze(0), SR)
            writer.writerow([fname, round(freq, 2)])

if __name__ == "__main__":
    cook()