# SD-card theme assets

DeskClock S3 can load personal theme images from the board's FAT32 microSD/TF card at runtime. These files are normal image files, not firmware-linked C arrays, so replacing them does **not** require rebuilding or reflashing firmware.

## SD-card layout

Copy this layout to the root of the microSD card:

```text
deskclock/
  themes/
    dusk/
      background.jpg
      mascot.png
      weather.png
      focus.png
      message.png
    blush/
      background.jpg
      mascot.png
      weather.png
      focus.png
      message.png
    studio/
      background.jpg
      mascot.png
      weather.png
      focus.png
      message.png
  common/
    weather.png
    focus.png
    message.png
```

Theme-specific files win over `common/` files. Missing files are OK; the firmware falls back to the built-in neutral LVGL-drawn shapes.

## Supported files

- Backgrounds: `background.jpg` or `background.jpeg`
  - Recommended size: `640×172`.
  - JPG is preferred because LVGL can stream-decode it from SD with much less RAM.
- Mascots/icons: `mascot.png`, `weather.png`, `focus.png`, `message.png` with optional transparency.
  - Keep these near their displayed size.
  - Runtime PNG loading is capped to protect device memory.

## Prepare the current background set

The helper script creates an SD-card folder from the downloaded ChatGPT background references. It uses Pillow (`python3 -m pip install pillow` if needed):

```sh
python3 scripts/prepare_sd_assets.py
```

It writes:

```text
.pio/sdcard/deskclock/themes/dusk/background.jpg
.pio/sdcard/deskclock/themes/blush/background.jpg
.pio/sdcard/deskclock/themes/studio/background.jpg
```

Then copy the contents of `.pio/sdcard` to the root of a FAT32 microSD card.

You can merge personal mascot/icon assets without committing them:

```sh
python3 scripts/prepare_sd_assets.py --personal-assets /path/to/personal-assets
```

where `/path/to/personal-assets` mirrors the SD layout, e.g. `deskclock/themes/dusk/mascot.png`.

## Hardware notes

The Waveshare ESP32-S3-Touch-LCD-3.49 SD slot is mounted as `/sdcard` using SDMMC 1-bit mode:

- CLK: GPIO41
- CMD: GPIO39
- D0: GPIO40

LVGL sees this as the `S:` drive, so firmware paths look like `S:/deskclock/themes/dusk/background.jpg`.
