import os
import random
import csv
import torch
import torchaudio
import numpy as np
from scipy.signal import fftconvolve, iirfilter, lfilter, welch
from tqdm import tqdm

# --- Configuration ---
NUM_SAMPLES = 1 
SR = 48000
VOCALS_DIR = './raw_data/vocals/VCTK-Corpus-0.92' 
IR_DIR = './raw_data/impulses/AIR_1_4' 
OUT_DIR = './feedback_study_data'
LABEL_FILE = os.path.join(OUT_DIR, 'labels.csv')

os.makedirs(OUT_DIR, exist_ok=True)

def get_files(path, extensions=('.wav', '.flac')):
    return [os.path.join(dp, f) for dp, dn, filenames in os.walk(path) for f in filenames if f.lower().endswith(extensions)]

def get_ir_peak(ir_audio, sr):
    # Uses Welch's method to find the dominant frequency in the Room IR
    f, Pxx = welch(ir_audio, fs=sr, nperseg=1024)
    peak_idx = np.argmax(Pxx)
    return f[peak_idx]

def cook_recursive_data():
    vocals = get_files(VOCALS_DIR)
    irs = get_files(IR_DIR)
    
    with open(LABEL_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['filename', 'target_freq_hz', 'ir_source'])
        
        for i in tqdm(range(NUM_SAMPLES), desc="Cooking Whisps"):
            v_path = random.choice(vocals)
            ir_path = random.choice(irs)
            
            # 1. Load Assets
            dry, _ = torchaudio.load(v_path)
            ir, _ = torchaudio.load(ir_path)
            
            # Get 1 second of dry vocal
            vocal_len = SR
            dry_vocal = dry[0, :vocal_len].numpy()
            
            # Analyze the IR to find the room's worst resonance
            ir_sig = ir[0, :].numpy()
            target_freq = get_ir_peak(ir_sig, SR)
            
            # Constrain to realistic feedback zones (250Hz - 6kHz)
            if target_freq < 250 or target_freq > 6000:
                target_freq = random.uniform(400, 4000)
            
            # 2. Setup the Canvas (1s Vocal + 2.5s Silence)
            total_len = int(SR * 3.5)
            canvas = np.zeros(total_len)
            canvas[:vocal_len] = dry_vocal
            
            # 3. Apply the Natural Room (Convolve ONCE)
            # We scale the IR by 0.05 (-26dB) to counteract the dataset normalization
            room_ir = ir_sig[:int(SR*0.5)] * 0.05 
            room_reverb = fftconvolve(canvas, room_ir, mode='full')[:total_len]
            
            # Mix Dry + Reverb
            mixed_audio = canvas + room_reverb
            
            # 4. Create the "Whisp" (Closed-Loop Pole Simulation)
            # High Q simulates the system sitting right on the edge of stability
            q = random.uniform(100, 200) 
            b, a = iirfilter(2, [target_freq * 0.99, target_freq * 1.01], rs=3, ftype='band', fs=SR, btype='bandpass')
            
            # Filter the audio to isolate the ringing
            ringing_tail = lfilter(b, a, mixed_audio)
            
            # 5. Add the "Breathing" Envelope
            t = np.arange(total_len) / SR
            lfo_rate = random.uniform(0.5, 2.0) # Hz
            breath_env = 0.6 + 0.4 * np.sin(2 * np.pi * lfo_rate * t) # Wavers between 0.2 and 1.0
            
            # Apply breathing and mix back into the main audio
            whisp_gain = random.uniform(1.5, 3.0) 
            final_audio = mixed_audio + (ringing_tail * breath_env * whisp_gain)
            
            # 6. Safe Normalization (Targeting -3dB to preserve dynamics)
            final_audio = final_audio / np.max(np.abs(final_audio)) * 0.707
            
            # Save
            fname = f"ring_{i:05d}.wav"
            torchaudio.save(os.path.join(OUT_DIR, fname), torch.tensor(final_audio, dtype=torch.float32).unsqueeze(0), SR)
            writer.writerow([fname, round(target_freq, 2), os.path.basename(ir_path)])

if __name__ == "__main__":
    cook_recursive_data()