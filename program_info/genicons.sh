#!/bin/sh
# Regenerate PXL assets with the optional Qt renderer and Pillow developer tool.
set -eu
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /path/to/pxl-branding-render" >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
temporary_png=$(mktemp)
trap 'rm -f "$temporary_png"' EXIT HUP INT TERM
"$1" "$script_dir/PXLConeLauncher.icon/assets/logo/pxlcone-icon.svg" "$temporary_png"
python3 "$script_dir/branding-tools/package.py" "$temporary_png" "$script_dir/pxlcone.ico" "$script_dir/pxlcone_256.png" --icns "$script_dir/pxlcone.icns"
