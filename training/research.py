import os
import requests
import zipfile
import torch
import torchaudio # REQUIRED: This was the name error
from tqdm import tqdm

# Setup directories
root_dir = './raw_data'
vocals_dir = os.path.join(root_dir, 'vocals')
impulses_dir = os.path.join(root_dir, 'impulses')
os.makedirs(vocals_dir, exist_ok=True)
os.makedirs(impulses_dir, exist_ok=True)

# 1. NEW IR SOURCE (Aachen Impulse Response - AIR)
# This link is hosted by the university and is much more stable than EchoThief.
air_url = 'https://zenodo.org/record/5356943/files/AIR_1_4.zip'

def download_file(url, dest):
    print(f"Connecting to {url}...")
    try:
        response = requests.get(url, stream=True, timeout=15)
        response.raise_for_status()
        total_size = int(response.headers.get('content-length', 0))
        with open(dest, 'wb') as f:
            with tqdm(total=total_size, unit='B', unit_scale=True, desc="Downloading IRs") as pbar:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)
                    pbar.update(len(chunk))
        return True
    except Exception as e:
        print(f"Download failed: {e}")
        return False

zip_path = 'impulses.zip'
if not os.path.exists(os.path.join(impulses_dir, 'AIR_1_4')):
    if download_file(air_url, zip_path):
        print("Extracting IRs...")
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(impulses_dir)
        os.remove(zip_path)

# 2. VCTK DOWNLOAD (Fixed Import)
print("\nInitializing VCTK Download...")
try:
    # Now that 'torchaudio' is imported, this won't crash.
    # Note: VCTK is HUGE (11GB+). If you want a quick test, use YESNO instead.
    dataset = torchaudio.datasets.VCTK_092(root=vocals_dir, download=True)
    print("VCTK check passed.")
except Exception as e:
    print(f"VCTK Error: {e}")