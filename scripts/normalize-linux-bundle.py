#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Normalize a new sharun bundle to its selected Qt SDK and relative asset paths."""

import argparse
from pathlib import Path, PurePosixPath
import shutil


def normalize_bundle(appdir: Path, qt_prefix: str, backup: Path) -> None:
    root = appdir.resolve(strict=True)
    prefix = PurePosixPath(qt_prefix)
    if not prefix.is_absolute() or '..' in prefix.parts or len(prefix.parts) < 2:
        raise ValueError('Qt prefix must be an absolute SDK prefix')
    lib = root / 'shared/lib'
    bundled_qt = lib.joinpath(*prefix.parts[1:])
    for path in (lib, bundled_qt, root / 'share', root / 'bin', root / 'shared/bin'):
        resolved = path.resolve(strict=True)
        if not resolved.is_dir() or root not in resolved.parents:
            raise ValueError('Expected directories inside a clean sharun bundle')
    if not (root / 'bin/pxlcone').is_file():
        raise ValueError('Missing bundled launcher')
    if not (bundled_qt / 'lib/libQt6Core.so.6').is_file() or not (bundled_qt / 'plugins/platforms').is_dir():
        raise ValueError('The selected Qt SDK libraries/plugins were not bundled')
    destination = backup.absolute()
    if destination.exists() or destination.is_symlink():
        raise ValueError('Preserve existing backup; choose a new path')
    destination = destination.parent.resolve(strict=True) / destination.name
    if destination == root or root in destination.parents:
        raise ValueError('Backup must be outside the distributable')
    if (lib / 'qt').exists() or (lib / 'qt').is_symlink() or (root / 'shared/share').exists() or (root / 'shared/share').is_symlink():
        raise ValueError('Expected a fresh bundle, not a previously normalized package')
    for path in bundled_qt.rglob('*'):
        if path.is_symlink() and root not in path.resolve(strict=True).parents:
            raise ValueError('Selected Qt tree contains an external symbolic link')

    destination.mkdir()
    # lib4bin can prefer system Qt. Retain it outside the package, then select one SDK.
    for path in lib.glob('libQt6*'):
        shutil.move(str(path), str(destination / path.name))
    for path in (bundled_qt / 'lib').iterdir():
        target = lib / path.name
        if target.exists() or target.is_symlink():
            shutil.move(str(target), str(destination / ('old-' + path.name)))
        if path.is_symlink():
            target.symlink_to(path.resolve(strict=True).name)
        else:
            shutil.copy2(path, target)
    (lib / 'qt').mkdir()
    # Plugin links may refer to the old SDK depth or absolute in-bundle paths.
    # External targets were rejected above; copy their contents before moving the SDK.
    shutil.copytree(bundled_qt / 'plugins', lib / 'qt/plugins', symlinks=False)
    parent = bundled_qt.parent
    shutil.move(str(bundled_qt), str(destination / 'sdk-tree'))
    # Remove only now-empty generated ancestors, never unrelated directories.
    while parent != lib:
        try:
            parent.rmdir()
        except OSError:
            break
        parent = parent.parent
    directories = sorted({p.parent.relative_to(lib).as_posix() for p in lib.rglob('*') if p.is_file()})
    (lib / 'lib.path').write_text('\n'.join('+' if p == '.' else '+/' + p for p in directories) + '\n', encoding='utf-8')
    for directory, relative in ((root / 'bin', '../shared/lib/qt'), (root / 'shared/bin', '../lib/qt')):
        (directory / 'qt.conf').write_text(f'[Paths]\nPrefix = {relative}\nPlugins = plugins\n', encoding='utf-8')
    # QApplication sees shared/bin after sharun dispatch; keep Java helpers/assets reachable.
    (root / 'shared/share').symlink_to('../share', target_is_directory=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('appdir', type=Path)
    parser.add_argument('--qt-prefix', required=True)
    parser.add_argument('--backup', required=True, type=Path)
    args = parser.parse_args()
    normalize_bundle(args.appdir, args.qt_prefix, args.backup)
