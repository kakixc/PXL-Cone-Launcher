# SPDX-License-Identifier: GPL-3.0-only
"""Package a Qt-rendered transparent PXL PNG into native icon formats (developer tool)."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source_png", type=Path)
parser.add_argument("ico", type=Path)
parser.add_argument("png256", type=Path)
parser.add_argument("--icns", type=Path)
args = parser.parse_args()
image = Image.open(args.source_png).convert("RGBA")
if image.size != (1024, 1024) or image.getpixel((0, 0))[3] != 0:
    parser.error("Expected a transparent 1024x1024 PNG from pxl-branding-render")
sizes = {(n, n) for n in (16, 24, 32, 48, 64, 128, 256)}
image.save(args.ico, format="ICO", sizes=sorted(sizes))
image.resize((256, 256), Image.Resampling.LANCZOS).save(args.png256)
if args.icns:
    image.save(args.icns, format="ICNS")
if Image.open(args.ico).ico.sizes() != sizes:
    raise RuntimeError("ICO is missing required native sizes")
