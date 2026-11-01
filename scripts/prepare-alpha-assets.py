#!/usr/bin/env python3
"""Stage reviewed alpha packages locally. Never builds or publishes a release."""

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import tempfile
import zipfile


def stage_assets(portable: Path, setup: Path, appimage: Path, output: Path, version: str) -> list[Path]:
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-alpha\.(0|[1-9][0-9]*)", version):
        raise ValueError("Use an explicit alpha version, for example 0.1.0-alpha.1")
    portable = portable.resolve(strict=True)
    setup = setup.resolve(strict=True)
    appimage = appimage.resolve(strict=True)
    output = output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError("Output must be a new directory")
    if not output.parent.is_dir():
        raise ValueError("Output parent directory must already exist")
    output = output.parent.resolve() / output.name
    if output == portable or portable in output.parents:
        raise ValueError("Output must be outside the portable payload")
    if not portable.is_dir():
        raise ValueError("Portable payload must be a directory")
    for file, magic in ((setup, b"MZ"), (appimage, b"\x7fELF")):
        with file.open("rb") as stream:
            if stream.read(len(magic)) != magic:
                raise ValueError("Setup must be a Windows executable and AppImage must be an ELF file")
    for required in ("pxlcone.exe", "portable.txt", "manifest.txt"):
        if not (portable / required).is_file():
            raise ValueError(f"Missing portable payload file: {required}")

    files = sorted(portable.rglob("*"))
    private_names = {"userdata", "instances", "logs", "accounts.json", "pxlcone.cfg", "prismlauncher.cfg",
                     "elyprismlauncher.cfg", "polymc.cfg", "multimc.cfg", ".git"}
    for file in files:
        relative = file.relative_to(portable)
        if file.is_symlink():
            raise ValueError("Windows portable payload cannot contain symbolic links")
        if any(part.lower() in private_names for part in relative.parts):
            raise ValueError(f"Personal data or checkout found in portable payload: {relative}")
        if re.fullmatch(r"PXL-Cone-(Test|Diagnostic).*\.exe", file.name, re.IGNORECASE):
            raise ValueError("Use a clean package payload, not the development preview folder")

    names = [f"PXL-Cone-Windows-x64-Portable-{version}.zip",
             f"PXL-Cone-Windows-x64-Setup-{version}.exe",
             f"PXL-Cone-Linux-x86_64-{version}.AppImage"]
    with tempfile.TemporaryDirectory(prefix=".pxl-alpha-", dir=output.parent) as temporary:
        staging = Path(temporary) / "assets"
        staging.mkdir()
        with zipfile.ZipFile(staging / names[0], "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for file in files:
                if file.is_file():
                    archive.write(file, file.relative_to(portable).as_posix())
        shutil.copy2(setup, staging / names[1])
        shutil.copy2(appimage, staging / names[2])
        (staging / names[2]).chmod(0o755)
        checksums = []
        for name in names:
            digest = hashlib.sha256()
            with (staging / name).open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            checksums.append(f"{digest.hexdigest()}  {name}")
        with (staging / "SHA256SUMS").open("w", encoding="utf-8", newline="\n") as checksum_file:
            checksum_file.write("\n".join(checksums) + "\n")
        # Reserve the destination atomically; rename() can replace an empty
        # directory created concurrently on POSIX. Never overwrite that folder.
        output.mkdir()
        for name in names + ["SHA256SUMS"]:
            with (staging / name).open("rb") as source, (output / name).open("xb") as destination:
                shutil.copyfileobj(source, destination)
            shutil.copystat(staging / name, output / name)
    return [output / name for name in names] + [output / "SHA256SUMS"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("portable", "setup", "appimage", "output"):
        parser.add_argument(f"--{option}", required=True, type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    try:
        paths = stage_assets(args.portable, args.setup, args.appimage, args.output, args.version)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Cannot stage alpha assets: {error}\n")
    for path in paths:
        print(path)
