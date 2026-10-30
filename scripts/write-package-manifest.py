#!/usr/bin/env python3
"""Write the updater's UTF-8 relative-file manifest for a Linux package."""

import argparse
import os
from pathlib import Path


def write_manifest(package_root: Path) -> None:
    root = package_root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError("Package root must be a directory")
    output = root / "manifest.txt"
    if output.is_symlink():
        raise ValueError("Package manifest cannot be a symbolic link")
    names = {"manifest.txt"}
    for directory, directories, files in os.walk(root, followlinks=False):
        base = Path(directory)
        entries = files + [name for name in directories if (base / name).is_symlink()]
        for name in entries:
            if not (base / name).is_file() and not (base / name).is_symlink():
                continue
            relative = (base / name).relative_to(root).as_posix()
            if "\n" in relative or "\r" in relative:
                raise ValueError("Package filenames cannot contain line breaks")
            names.add(relative)
    output.write_text("\n".join(sorted(names)) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package_root", type=Path)
    write_manifest(parser.parse_args().package_root)
