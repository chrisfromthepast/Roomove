import os
import csv
import torch
import torchaudio
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from tqdm import tqdm

# --- Configuration ---
DATA_DIR = './paired_room_data'
LABEL_FILE = os.path.join(DATA_DIR, 'labels.csv')
CHUNK_SIZE = 32768  # ~680ms for better context
N_FFT = 1024
HOP_LENGTH = 256
BATCH_SIZE = 16
EPOCHS = 30
LEARNING_RATE = 1e-4
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'mps' if torch.backends.mps.is_available() else 'cpu')

class SpectrogramDataset(Dataset):
    def __init__(self, csv_file, root_dir, chunk_size):
        self.root_dir = root_dir
        self.chunk_size = chunk_size
        self.pairs = []
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                self.pairs.append((row['wet_file'], row['dry_file']))

    def __len__(self):
        return len(self.pairs)

    def __getitem__(self, idx):
        wet_path = os.path.join(self.root_dir, self.pairs[idx][0])
        dry_path = os.path.join(self.root_dir, self.pairs[idx][1])
        
        wet_sig, _ = torchaudio.load(wet_path)
        dry_sig, _ = torchaudio.load(dry_path)
        
        # Slicing
        max_start = wet_sig.shape[1] - self.chunk_size
        start_idx = torch.randint(0, max_start, (1,)).item() if max_start > 0 else 0
        
        wet_chunk = wet_sig[:, start_idx:start_idx + self.chunk_size]
        dry_chunk = dry_sig[:, start_idx:start_idx + self.chunk_size]
        
        # Convert to Spectrograms (Magnitude only for the model)
        # We use power=1 for magnitude spectrogram
        spec_transform = torchaudio.transforms.Spectrogram(n_fft=N_FFT, hop_length=HOP_LENGTH, power=1).to(wet_chunk.device)
        
        wet_spec = spec_transform(wet_chunk)
        dry_spec = spec_transform(dry_chunk)
        
        return wet_spec, dry_spec

class SpecUNet(nn.Module):
    def __init__(self):
        super(SpecUNet, self).__init__()
        # Simple Encoder/Decoder for 2D Spectrograms
        self.enc1 = nn.Conv2d(1, 32, kernel_size=3, stride=2, padding=1)
        self.enc2 = nn.Conv2d(32, 64, kernel_size=3, stride=2, padding=1)
        self.dec1 = nn.ConvTranspose2d(64, 32, kernel_size=3, stride=2, padding=1, output_padding=1)
        self.dec2 = nn.ConvTranspose2d(32, 1, kernel_size=3, stride=2, padding=1, output_padding=1)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid() # Masks should be 0.0 to 1.0

    def forward(self, x):
        # We predict a MASK that multiplies against the input
        orig_input = x
        x = self.relu(self.enc1(x))
        x = self.relu(self.enc2(x))
        x = self.relu(self.dec1(x))
        mask = self.sigmoid(self.dec2(x))
        
        # Ensure dimensions match for the mask multiplication
        mask = mask[:, :, :orig_input.shape[2], :orig_input.shape[3]]
        return orig_input * mask

def train_spec():
    print(f"--- Training Spectrogram U-Net on {DEVICE} ---")
    dataset = SpectrogramDataset(LABEL_FILE, DATA_DIR, CHUNK_SIZE)
    train_loader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)
    
    model = SpecUNet().to(DEVICE)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)

    for epoch in range(EPOCHS):
        model.train()
        for wet_spec, dry_spec in tqdm(train_loader, desc=f"Epoch {epoch+1}"):
            wet_spec, dry_spec = wet_spec.to(DEVICE), dry_spec.to(DEVICE)
            
            optimizer.zero_grad()
            output_spec = model(wet_spec)
            loss = criterion(output_spec, dry_spec)
            loss.backward()
            optimizer.step()
        
        print(f"Epoch {epoch+1} Loss: {loss.item():.6f}")
        torch.save(model.state_dict(), "spec_subtractor.pth")

if __name__ == "__main__":
    train_spec()