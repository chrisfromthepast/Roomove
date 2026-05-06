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
CHUNK_SIZE = 32768  
N_FFT = 2048        # Higher frequency resolution for room modes
HOP_LENGTH = 512
BATCH_SIZE = 16
EPOCHS = 50         # Targeted run length
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
        
        max_start = wet_sig.shape[1] - self.chunk_size
        start_idx = torch.randint(0, max_start, (1,)).item() if max_start > 0 else 0
        
        wet_chunk = wet_sig[:, start_idx:start_idx + self.chunk_size]
        dry_chunk = dry_sig[:, start_idx:start_idx + self.chunk_size]
        
        # FFT Transformation
        spec_transform = torchaudio.transforms.Spectrogram(n_fft=N_FFT, hop_length=HOP_LENGTH, power=1).to(DEVICE)
        
        # Power-law compression (0.3) makes quiet reverb tails as important as loud peaks
        wet_spec = spec_transform(wet_chunk.to(DEVICE)).pow(0.3)
        dry_spec = spec_transform(dry_chunk.to(DEVICE)).pow(0.3)
        
        return wet_spec, dry_spec

class SpecUNet(nn.Module):
    def __init__(self):
        super(SpecUNet, self).__init__()
        # Encoder
        self.enc1 = nn.Conv2d(1, 32, kernel_size=3, stride=2, padding=1)
        self.enc2 = nn.Conv2d(32, 64, kernel_size=3, stride=2, padding=1)
        
        # Decoder
        self.dec1 = nn.ConvTranspose2d(64, 32, kernel_size=3, stride=2, padding=1, output_padding=1)
        # dec2 takes 32 (from dec1) + 32 (skip connection from enc1)
        self.dec2 = nn.ConvTranspose2d(64, 1, kernel_size=3, stride=2, padding=1, output_padding=1)
        
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        # Encoder path
        e1 = self.relu(self.enc1(x))
        e2 = self.relu(self.enc2(e1))
        
        # Decoder path with Skip Connection
        d1 = self.relu(self.dec1(e2))
        
        # Align dimensions for concat
        d1 = d1[:, :, :e1.shape[2], :e1.shape[3]]
        skip_connection = torch.cat([d1, e1], dim=1)
        
        mask = self.sigmoid(self.dec2(skip_connection))
        
        # Align mask to original input
        mask = mask[:, :, :x.shape[2], :x.shape[3]]
        return x * mask

def train():
    print(f"--- Booting 50-Epoch Skip-U-Net on {DEVICE} ---")
    dataset = SpectrogramDataset(LABEL_FILE, DATA_DIR, CHUNK_SIZE)
    
    train_size = int(0.9 * len(dataset))
    val_size = len(dataset) - train_size
    train_dataset, val_dataset = torch.utils.data.random_split(dataset, [train_size, val_size])
    
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)

    model = SpecUNet().to(DEVICE)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    
    # Scheduler reduces LR when validation loss plateaus
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, 'min', patience=3, factor=0.5)

    best_val_loss = float('inf')

    for epoch in range(EPOCHS):
        model.train()
        train_loss = 0
        for wet, dry in tqdm(train_loader, desc=f"Epoch {epoch+1}/{EPOCHS}"):
            optimizer.zero_grad()
            output = model(wet)
            loss = criterion(output, dry)
            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        # Validation
        model.eval()
        val_loss = 0
        with torch.no_grad():
            for wet, dry in val_loader:
                output = model(wet)
                val_loss += criterion(output, dry).item()
        
        avg_val_loss = val_loss / len(val_loader)
        scheduler.step(avg_val_loss)
        
        print(f"Epoch {epoch+1}: Train Loss {train_loss/len(train_loader):.6f} | Val Loss {avg_val_loss:.6f}")
        
        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            torch.save(model.state_dict(), "spec_subtractor.pth")
            print("   [!] Best Model Updated.")

if __name__ == "__main__":
    train()