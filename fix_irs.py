import os
import scipy.io
from scipy.io import wavfile
import numpy as np

ir_dir = './raw_data/impulses/AIR_1_4'

print("Cracking open MATLAB files and extracting .wav audio...")

# Walk through the folder looking for .mat files
found = 0
for root, dirs, files in os.walk(ir_dir):
    for file in files:
        if file.lower().endswith('.mat'):
            mat_path = os.path.join(root, file)
            try:
                # Load the MATLAB file
                mat_data = scipy.io.loadmat(mat_path)
                
                # Find the actual audio array (ignoring MATLAB headers)
                for key, val in mat_data.items():
                    if not key.startswith('__') and isinstance(val, np.ndarray):
                        # Flatten to 1D and normalize
                        audio = np.squeeze(val).astype(np.float32)
                        audio = audio / (np.max(np.abs(audio)) + 1e-6)
                        
                        # Save as .wav
                        wav_path = os.path.join(root, file.lower().replace('.mat', '.wav'))
                        wavfile.write(wav_path, 48000, audio)
                        found += 1
                        break # Move to the next file once we have the audio
            except Exception as e:
                print(f"Skipped {file}: {e}")

print(f"Done! Successfully extracted {found} .wav Impulse Responses.")
print("You are clear to run: python synthesize_data.py")