import os
import requests
import zipfile
from tqdm import tqdm

# Just the impulses
impulses_dir = './raw_data/impulses'
os.makedirs(impulses_dir, exist_ok=True)
air_url = 'https://www.openslr.org/resources/20/air_database_release_1_4.zip'

print("Downloading Aachen IRs from OpenSLR...")
r = requests.get(air_url, stream=True)
with open('irs.zip', 'wb') as f:
    total = int(r.headers.get('content-length', 0))
    with tqdm(total=total, unit='B', unit_scale=True) as pbar:
        for chunk in r.iter_content(8192):
            f.write(chunk)
            pbar.update(len(chunk))

with zipfile.ZipFile('irs.zip', 'r') as z:
    z.extractall(impulses_dir)
os.remove('irs.zip')
print("Done.")