#!/usr/bin/env python3
"""Build only public website assets, without copying launcher or user data."""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "index.html": "website/index.html",
    "styles.css": "website/styles.css",
    "app.js": "website/app.js",
    "downloads.js": "website/downloads.js",
    "assets/pxlcone.svg": "program_info/PXLConeLauncher.icon/assets/logo/pxlcone-icon.svg",
    "assets/download.svg": "launcher/resources/pxl/icons/download-simple.svg",
    "assets/Inter.ttf": "launcher/resources/fonts/Inter.ttf",
    "assets/Inter-OFL.txt": "launcher/resources/fonts/Inter-OFL.txt",
    "assets/Phosphor-LICENSE.txt": "launcher/resources/pxl/icons/LICENSE.txt",
    "assets/landscape.png": "launcher/resources/pxl/cone-dawn-banner-v1.png",
    **{f"assets/{name}.png": f"docs/images/screenshots/{name}.png"
       for name in ("home-library", "content-browser", "share-pack")},
}


def build(destination: Path) -> None:
    if destination.is_symlink():
        raise ValueError("Output must not be a symlink")
    destination = destination.resolve()
    if destination == ROOT or ROOT not in destination.parents or not destination.name.startswith("build-"):
        raise ValueError("Output must be a build-* directory inside the repository")
    allowed = set(FILES) | {".nojekyll"}
    allowed_directories = {parent.as_posix() for name in allowed for parent in Path(name).parents
                           if parent != Path(".")}
    if destination.exists():
        unexpected = []
        for item in destination.rglob("*"):
            relative = item.relative_to(destination).as_posix()
            if item.is_symlink() or not ((item.is_file() and relative in allowed)
                                         or (item.is_dir() and relative in allowed_directories)):
                unexpected.append(relative)
        if unexpected:
            raise ValueError(f"Output contains unrelated files; choose a fresh build-* directory: {unexpected}")
    for target, source in FILES.items():
        source_path = ROOT / source
        if not source_path.is_file():
            raise FileNotFoundError(source_path)
        target_path = destination / target
        if target_path.is_symlink() or any(parent.is_symlink() for parent in target_path.parents if parent != ROOT):
            raise ValueError("Output must not contain symlinks")
        target_path.parent.mkdir(parents=True, exist_ok=True)
        if target in ("index.html", "styles.css"):
            text = source_path.read_text(encoding="utf-8")
            for asset_target, asset_source in FILES.items():
                if asset_target.startswith("assets/"):
                    text = text.replace("../" + asset_source, asset_target)
            target_path.write_text(text, encoding="utf-8")
        else:
            shutil.copyfile(source_path, target_path)
    (destination / ".nojekyll").write_text("", encoding="utf-8")
    print(f"Built {len(FILES)} public files in {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="build-pxl-site")
    options = parser.parse_args()
    build(ROOT / options.output)
