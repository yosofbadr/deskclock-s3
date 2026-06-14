#!/usr/bin/env python3
"""Copy prepared DeskClock assets onto a mounted FAT32 microSD card.

Usage:
  python3 scripts/copy_sd_assets.py
  python3 scripts/copy_sd_assets.py /Volumes/DESKCLOCK

Run scripts/prepare_sd_assets.py first. This script copies the staged deskclock/
folder to the card root, removes macOS AppleDouble sidecar files, and verifies
copied file hashes.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
from pathlib import Path

DEFAULT_VOLUME = Path("/Volumes/DESKCLOCK")


def remove_macos_sidecars(root: Path) -> None:
    for path in root.rglob("*"):
        if path.name == ".DS_Store" or path.name.startswith("._"):
            path.unlink(missing_ok=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_copy(source: Path, destination: Path) -> int:
    checked = 0
    failures: list[str] = []
    for source_file in sorted(path for path in source.rglob("*") if path.is_file()):
        relative = source_file.relative_to(source)
        destination_file = destination / relative
        if not destination_file.is_file():
            failures.append(f"missing {destination_file}")
            continue
        checked += 1
        if sha256(source_file) != sha256(destination_file):
            failures.append(f"hash mismatch {destination_file}")

    if failures:
        details = "\n".join(f"- {failure}" for failure in failures)
        raise SystemExit(f"copy verification failed:\n{details}")
    return checked


def resolve_volume(value: Path | None) -> Path:
    if value is not None:
        return value
    if DEFAULT_VOLUME.is_dir():
        return DEFAULT_VOLUME
    mounted = sorted(path.name for path in Path("/Volumes").iterdir() if path.name != "Macintosh HD") if Path("/Volumes").is_dir() else []
    hint = f" Mounted volumes: {', '.join(mounted)}." if mounted else " No removable volumes appear to be mounted."
    raise SystemExit(f"No volume specified and {DEFAULT_VOLUME} is not mounted.{hint}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("volume", type=Path, nargs="?", help="Mounted SD-card volume, e.g. /Volumes/DESKCLOCK")
    parser.add_argument("--source", type=Path, default=Path(".pio/sdcard"), help="Prepared SD-card staging folder")
    args = parser.parse_args()

    source_root = args.source
    source_deskclock = source_root / "deskclock"
    volume = resolve_volume(args.volume)
    destination = volume / "deskclock"

    if not source_deskclock.is_dir():
        raise SystemExit(f"missing prepared assets: {source_deskclock}; run scripts/prepare_sd_assets.py first")
    if not volume.is_dir():
        raise SystemExit(f"volume is not mounted or is not a directory: {volume}")
    if not os.access(volume, os.W_OK):
        raise SystemExit(f"volume is not writable: {volume}")

    shutil.copytree(source_deskclock, destination, dirs_exist_ok=True)
    remove_macos_sidecars(destination)
    checked = verify_copy(source_deskclock, destination)

    print(f"Copied {source_deskclock} -> {destination}")
    print(f"Verified {checked} files")
    print("You can now eject the card and insert it into the DeskClock device.")


if __name__ == "__main__":
    main()
