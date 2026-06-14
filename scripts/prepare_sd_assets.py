#!/usr/bin/env python3
"""Prepare a microSD asset folder for DeskClock S3.

This does not generate firmware-linked C assets. It only copies/resizes normal
image files into the SD-card directory layout that the firmware loads at runtime.

Default input expects the ChatGPT background images previously downloaded to:
  .pio/references/background-1.png  # dusk / Kuromi-style room
  .pio/references/background-2.png  # blush / Hello Kitty-style room
  .pio/references/background-3.png  # studio / Miffy-style room

Default output is:
  .pio/sdcard/deskclock/themes/<theme>/background.jpg

Copy the contents of .pio/sdcard to the root of a FAT32 microSD card.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path
from PIL import Image, ImageFilter

DISPLAY_SIZE = (640, 172)

THEME_BACKGROUNDS = {
    "dusk": "background-1.png",
    "blush": "background-2.png",
    "studio": "background-3.png",
}


def fit_on_blurred_canvas(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Fit the full artwork without cropping, filling side gaps from the image itself."""
    target_w, target_h = size
    src_w, src_h = image.size

    cover_scale = max(target_w / src_w, target_h / src_h)
    cover = image.resize((round(src_w * cover_scale), round(src_h * cover_scale)), Image.Resampling.LANCZOS)
    cover_left = max(0, (cover.width - target_w) // 2)
    cover_top = max(0, (cover.height - target_h) // 2)
    canvas = cover.crop((cover_left, cover_top, cover_left + target_w, cover_top + target_h)).filter(ImageFilter.GaussianBlur(18))

    contain_scale = min(target_w / src_w, target_h / src_h)
    contained = image.resize((round(src_w * contain_scale), round(src_h * contain_scale)), Image.Resampling.LANCZOS)
    left = (target_w - contained.width) // 2
    top = (target_h - contained.height) // 2
    canvas.paste(contained, (left, top))
    return canvas


def prepare_backgrounds(source_dir: Path, out_root: Path) -> None:
    for theme, filename in THEME_BACKGROUNDS.items():
        src = source_dir / filename
        if not src.exists():
            print(f"skip {theme}: missing {src}")
            continue
        out_dir = out_root / "deskclock" / "themes" / theme
        out_dir.mkdir(parents=True, exist_ok=True)
        with Image.open(src) as image:
            prepared = fit_on_blurred_canvas(image.convert("RGB"), DISPLAY_SIZE)
            prepared.save(out_dir / "background.jpg", quality=92, optimize=True, progressive=False)
        shutil.copy2(src, out_dir / "background-source.png")
        print(f"wrote {out_dir / 'background.jpg'}")


def copy_optional_assets(personal_assets: Path | None, out_root: Path) -> None:
    if personal_assets is None:
        return
    if not personal_assets.exists():
        raise SystemExit(f"optional asset source does not exist: {personal_assets}")

    # Expected layout mirrors the SD card layout, e.g.
    # personal-assets/deskclock/themes/dusk/mascot.png
    for path in personal_assets.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(personal_assets)
        destination = out_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        print(f"copied {destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--background-source", type=Path, default=Path(".pio/references"))
    parser.add_argument("--personal-assets", type=Path, default=None,
                        help="Optional folder to merge into the SD-card image. Layout should start with deskclock/...")
    parser.add_argument("--out", type=Path, default=Path(".pio/sdcard"))
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    prepare_backgrounds(args.background_source, args.out)
    copy_optional_assets(args.personal_assets, args.out)

    print("\nSD card folder ready:", args.out)
    print("Copy the contents of this folder to the root of a FAT32 microSD card.")


if __name__ == "__main__":
    main()
