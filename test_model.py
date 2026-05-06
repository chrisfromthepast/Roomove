import os
import torch
import torchaudio
import csv
import random
from train import SubtractorNet # Import your model class

# --- Configuration ---
DATA_DIR = './paired_room_data'
LABEL_FILE = os.path.join(DATA_DIR, 'labels.csv')
MODEL_PATH = 'subtractor_model_best.pth'
OUT_DIR = './test_results'
CHUNK_SIZE = 16384 
SR = 48000

os.makedirs(OUT_DIR, exist_ok=True)
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'mps' if torch.backends.mps.is_available() else 'cpu')

def run_test():
    # 1. Load the Model
    model = SubtractorNet().to(DEVICE)
    model.load_state_dict(torch.load(MODEL_PATH, map_location=DEVICE))
    model.eval()

    # 2. Pick a random file from the CSV
    with open(LABEL_FILE, 'r') as f:
        reader = list(csv.DictReader(f))
        test_case = random.choice(reader)
    
    wet_path = os.path.join(DATA_DIR, test_case['wet_file'])
    dry_path = os.path.join(DATA_DIR, test_case['dry_file'])
    
    print(f"Testing on: {test_case['wet_file']} (IR: {test_case['ir_source']})")

    # 3. Process the Audio
    wet_sig, _ = torchaudio.load(wet_path)
    
    # We process in chunks or the whole file if it's small enough
    with torch.no_grad():
        input_tensor = wet_sig.unsqueeze(0).to(DEVICE)
        # Ensure input length is a multiple of 16 for the ConvTranspose layers
        pad_len = 16 - (input_tensor.shape[2] % 16)
        input_tensor = torch.nn.functional.pad(input_tensor, (0, pad_len))
        
        output = model(input_tensor)
        output = output[:, :, :-pad_len] # Remove padding

    # 4. Save the results for comparison
    # Pulling the Ground Truth (original dry) to compare
    dry_sig, _ = torchaudio.load(dry_path)

    torchaudio.save(os.path.join(OUT_DIR, "0_input_wet.wav"), wet_sig, SR)
    torchaudio.save(os.path.join(OUT_DIR, "1_output_reconstructed.wav"), output.cpu().squeeze(0), SR)
    torchaudio.save(os.path.join(OUT_DIR, "2_target_ground_truth.wav"), dry_sig, SR)

    print(f"Results saved to {OUT_DIR}. Pull them down and listen!")

if __name__ == "__main__":
    run_test()