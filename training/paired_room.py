import os
import random
import csv
import torch
import torchaudio
import numpy as np
from scipy.signal import fftconvolve
from tqdm import tqdm
from concurrent.futures import ProcessPoolExecutor, as_completed

# --- Configuration ---
NUM_SAMPLES = 5000 
SR = 48000
VOCALS_DIR = './raw_data/vocals/VCTK-Corpus-0.92' 
IR_DIR = './raw_data/impulses/AIR_1_4' 
OUT_DIR = './paired_room_data' # Changed to keep things clean
LABEL_FILE = os.path.join(OUT_DIR, 'labels.csv')

os.makedirs(OUT_DIR, exist_ok=True)

def get_files(path, extensions=('.wav', '.flac')):
    return [os.path.join(dp, f) for dp, dn, filenames in os.walk(path) for f in filenames if f.lower().endswith(extensions)]

def process_sample(args):
    i, v_path, ir_path = args
    
    dry, _ = torchaudio.load(v_path)
    ir, _ = torchaudio.load(ir_path)
    
    # Cap dry vocal at 5 seconds
    dry_sig = dry[0, :].numpy()
    if len(dry_sig) > SR * 5: 
        dry_sig = dry_sig[:SR * 5]
        
    ir_sig = ir[0, :SR].numpy()
    
    # Convolve to get the wet signal
    wet_sig = fftconvolve(dry_sig, ir_sig, mode='full')
    
    # 1-to-1 Padding: Pad the dry signal with zeros to match the convolved tail length
    pad_len = len(wet_sig) - len(dry_sig)
    dry_sig_padded = np.pad(dry_sig, (0, pad_len), 'constant')
    
    # Relative Normalization
    # We find the absolute highest peak between BOTH files and scale them equally.
    # This preserves the natural volume loss that occurs when sound hits a room.
    max_val = max(np.max(np.abs(dry_sig_padded)), np.max(np.abs(wet_sig))) + 1e-6
    dry_sig_final = (dry_sig_padded / max_val) * 0.707
    wet_sig_final = (wet_sig / max_val) * 0.707
    
    dry_fname = f"dry_{i:05d}.wav"
    wet_fname = f"wet_{i:05d}.wav"
    
    torchaudio.save(os.path.join(OUT_DIR, dry_fname), torch.tensor(dry_sig_final, dtype=torch.float32).unsqueeze(0), SR)
    torchaudio.save(os.path.join(OUT_DIR, wet_fname), torch.tensor(wet_sig_final, dtype=torch.float32).unsqueeze(0), SR)
    
    return [f"{i:05d}", dry_fname, wet_fname, os.path.basename(ir_path)]

def cook_paired_data():
    vocals = get_files(VOCALS_DIR)
    irs = get_files(IR_DIR)
    
    tasks = [(i, random.choice(vocals), random.choice(irs)) for i in range(NUM_SAMPLES)]
    
    with open(LABEL_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['pair_id', 'dry_file', 'wet_file', 'ir_source'])
        
        with ProcessPoolExecutor() as executor:
            futures = {executor.submit(process_sample, task): task for task in tasks}
            
            for future in tqdm(as_completed(futures), total=NUM_SAMPLES, desc="Cooking Paired Data (Parallel)"):
                writer.writerow(future.result())

if __name__ == "__main__":
    cook_paired_data()