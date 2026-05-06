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
CHUNK_SIZE = 16384  # ~340ms at 48kHz
BATCH_SIZE = 32
EPOCHS = 30
LEARNING_RATE = 1e-4

# Automatically use Apple Silicon (MPS), Nvidia (CUDA), or fallback to CPU
DEVICE = torch.device('cuda' if torch.cuda.is_available() else 'mps' if torch.backends.mps.is_available() else 'cpu')

class PairedAudioDataset(Dataset):
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
        wet_fname, dry_fname = self.pairs[idx]
        
        wet_path = os.path.join(self.root_dir, wet_fname)
        dry_path = os.path.join(self.root_dir, dry_fname)
        
        wet_sig, _ = torchaudio.load(wet_path)
        dry_sig, _ = torchaudio.load(dry_path)
        
        # Randomly slice a chunk so the model learns continuous subtraction, not just the start of files
        max_start = wet_sig.shape[1] - self.chunk_size
        if max_start > 0:
            start_idx = torch.randint(0, max_start, (1,)).item()
        else:
            start_idx = 0
            
        wet_chunk = wet_sig[:, start_idx:start_idx + self.chunk_size]
        dry_chunk = dry_sig[:, start_idx:start_idx + self.chunk_size]
        
        # Pad if we hit a weird edge case
        if wet_chunk.shape[1] < self.chunk_size:
            pad_amt = self.chunk_size - wet_chunk.shape[1]
            wet_chunk = torch.nn.functional.pad(wet_chunk, (0, pad_amt))
            dry_chunk = torch.nn.functional.pad(dry_chunk, (0, pad_amt))
            
        return wet_chunk, dry_chunk

class SubtractorNet(nn.Module):
    def __init__(self):
        super(SubtractorNet, self).__init__()
        
        # Encoder: Compresses the audio to find the "Room Characteristics"
        self.encoder = nn.Sequential(
            nn.Conv1d(1, 16, kernel_size=15, stride=1, padding=7),
            nn.LeakyReLU(0.2),
            nn.Conv1d(16, 32, kernel_size=15, stride=4, padding=7),
            nn.LeakyReLU(0.2),
            nn.Conv1d(32, 64, kernel_size=15, stride=4, padding=7),
            nn.LeakyReLU(0.2)
        )
        
        # Decoder: Reconstructs the audio while ignoring the room math
        self.decoder = nn.Sequential(
            nn.ConvTranspose1d(64, 32, kernel_size=15, stride=4, padding=7, output_padding=3),
            nn.LeakyReLU(0.2),
            nn.ConvTranspose1d(32, 16, kernel_size=15, stride=4, padding=7, output_padding=3),
            nn.LeakyReLU(0.2),
            nn.Conv1d(16, 1, kernel_size=15, stride=1, padding=7),
            nn.Tanh() # Keeps audio between -1.0 and 1.0 safely
        )

    def forward(self, x):
        encoded = self.encoder(x)
        decoded = self.decoder(encoded)
        # Ensure exact sample matching due to ConvTranspose math quirks
        return decoded[:, :, :x.shape[2]]

def train():
    print(f"--- Booting Training on {DEVICE} ---")
    
    dataset = PairedAudioDataset(LABEL_FILE, DATA_DIR, CHUNK_SIZE)
    
    # 90% Training / 10% Validation Split
    train_size = int(0.9 * len(dataset))
    val_size = len(dataset) - train_size
    train_dataset, val_dataset = torch.utils.data.random_split(dataset, [train_size, val_size])
    
    # Num_workers speeds up data loading off the hard drive
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True, num_workers=4)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False, num_workers=4)

    model = SubtractorNet().to(DEVICE)
    
    # L1 Loss (Mean Absolute Error) usually sounds better for audio than MSE
    criterion = nn.L1Loss() 
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)

    best_val_loss = float('inf')

    for epoch in range(EPOCHS):
        model.train()
        running_loss = 0.0
        
        loop = tqdm(train_loader, desc=f"Epoch {epoch+1}/{EPOCHS} [Train]")
        for wet, dry in loop:
            wet, dry = wet.to(DEVICE), dry.to(DEVICE)
            
            optimizer.zero_grad()
            outputs = model(wet)
            loss = criterion(outputs, dry)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            loop.set_postfix(loss=loss.item())

        # Validation Phase
        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for wet, dry in tqdm(val_loader, desc=f"Epoch {epoch+1}/{EPOCHS} [Val]"):
                wet, dry = wet.to(DEVICE), dry.to(DEVICE)
                outputs = model(wet)
                val_loss += criterion(outputs, dry).item()
        
        avg_val_loss = val_loss / len(val_loader)
        print(f"-> Train Loss: {running_loss/len(train_loader):.4f} | Val Loss: {avg_val_loss:.4f}\n")
        
        # Save the best model
        if avg_val_loss < best_val_loss:
            best_val_loss = avg_val_loss
            torch.save(model.state_dict(), "subtractor_model_best.pth")
            print("   [!] New Best Model Saved.\n")

if __name__ == "__main__":
    train()