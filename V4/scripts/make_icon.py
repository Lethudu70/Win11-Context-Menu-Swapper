"""Render the editable app.svg with librsvg and encode a multi-size Windows ICO.
Requires UCRT64 librsvg and Pillow only for regeneration, not for building/running.
"""
from pathlib import Path
import argparse
import subprocess
from PIL import Image
parser = argparse.ArgumentParser()
parser.add_argument('--renderer', default='C:/msys64/ucrt64/bin/rsvg-convert.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
subprocess.run([args.renderer, '-w', '1024', '-h', '1024', '-o',
                str(root / 'resources/app.png'), str(root / 'resources/app.svg')], check=True)
with Image.open(root / 'resources/app.png') as im:
    im.save(root / 'resources/app.ico', sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
print(root / 'resources/app.ico')
