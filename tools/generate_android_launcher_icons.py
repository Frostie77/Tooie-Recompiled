#!/usr/bin/env python3
"""Regenerate committed Android launcher resources from the selected BT master.

Requires Pillow for regeneration only; normal Android builds use the committed
PNGs/XML and do not run this script. --check compares decoded PNG pixels so a
different PNG compression library does not create a false mismatch.
"""

import argparse
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "android/app/src/main/res"
BACKGROUND = "#000536"
SOURCE = ROOT / "assets/android/launcher-master.png"
# Match the accepted preview: the master occupies 88% of the 72 dp viewport.
ART_DP = 72 * 0.88
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}


def centered(source, size, art_size, background=(0, 0, 0, 0)):
    image = Image.new("RGBA", (size, size), background)
    art = source.resize((art_size, art_size), Image.Resampling.LANCZOS)
    position = ((size - art_size) // 2,) * 2
    image.alpha_composite(art, position)
    return image


def mask(size, shape):
    # Supersampling also keeps the legacy round edge smooth at 48 pixels.
    scale = 4
    result = Image.new("L", (size * scale, size * scale))
    draw = ImageDraw.Draw(result)
    box = (0, 0, size * scale - 1, size * scale - 1)
    if shape == "circle":
        draw.ellipse(box, fill=255)
    elif shape == "rounded":
        draw.rounded_rectangle(box, radius=size * scale // 4, fill=255)
    else:
        draw.rectangle(box, fill=255)
    return result.resize((size, size), Image.Resampling.LANCZOS)


def png(image):
    output = BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def resources(source):
    output = {}
    for density, scale in DENSITIES.items():
        size = round(48 * scale)
        legacy = centered(source, size, round(size * 0.88), BACKGROUND)
        legacy.putalpha(mask(size, "circle"))
        output[f"mipmap-{density}/ic_launcher.png"] = png(legacy)
        output[f"mipmap-{density}/ic_launcher_round.png"] = png(legacy)
        # Preserve the selected full-color master, including its navy field.
        # The colored Jiggy/letters fit the safe circle; Android may clip navy
        # corners of the square foreground without clipping the emblem.
        foreground = centered(source, round(108 * scale), round(ART_DP * scale))
        output[f"mipmap-{density}/ic_launcher_foreground.png"] = png(foreground)

    for api in (26, 33):
        # The master has an opaque navy field, so its alpha would produce a
        # solid square if reused for monochrome. Keep the colored icon instead.
        xml = ('<?xml version="1.0" encoding="utf-8"?>\n'
               '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
               '    <background android:drawable="@color/ic_launcher_background" />\n'
               '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
               '</adaptive-icon>\n')
        for name in ("ic_launcher", "ic_launcher_round"):
            output[f"mipmap-anydpi-v{api}/{name}.xml"] = xml.encode()
    output["values/launcher_icon.xml"] = (
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
        f'    <color name="ic_launcher_background">{BACKGROUND}</color>\n'
        '</resources>\n').encode()
    return output


def preview(source, path):
    sheet = Image.new("RGB", (540, 220), "#F2F2F6")
    font = ImageFont.load_default(size=15)
    draw = ImageDraw.Draw(sheet)
    full = centered(source, 432, round(ART_DP * 4), BACKGROUND)
    # Adaptive layers are 108 dp; the default mask viewport is the inner 72 dp.
    viewport = full.crop((72, 72, 360, 360)).resize((140, 140), Image.Resampling.LANCZOS)
    for x, size, label in ((20, 140, "Round"), (200, 48, "48 px"),
                           (380, 64, "64 px")):
        icon = viewport.copy()
        icon = icon.resize((size, size), Image.Resampling.LANCZOS)
        icon.putalpha(mask(size, "circle"))
        sheet.paste(icon, (x + (140 - size) // 2, 22 + (140 - size) // 2), icon)
        draw.text((x + 70, 182), label, fill=BACKGROUND, font=font, anchor="mm")
    draw.text((270, 206), "Tooie Recompiled", fill=BACKGROUND, font=font, anchor="mm")
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify without writing")
    parser.add_argument("--preview", type=Path, help="also render example launcher masks")
    args = parser.parse_args()
    with Image.open(SOURCE) as image:
        source = image.convert("RGBA")
    if source.width != source.height or source.width < 512:
        raise SystemExit("Expected the selected square BT master at least 512 pixels wide.")
    for relative, expected in resources(source).items():
        path = RES / relative
        if args.check:
            if not path.is_file():
                raise SystemExit(f"Missing: {path.relative_to(ROOT)}")
            if path.suffix == ".png":
                with Image.open(path) as actual, Image.open(BytesIO(expected)) as wanted:
                    matches = actual.size == wanted.size and actual.convert("RGBA").tobytes() == wanted.tobytes()
            else:
                matches = path.read_bytes() == expected
            if not matches:
                raise SystemExit(f"Out of date: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(expected)
    if args.preview:
        preview(source, args.preview)
    print(f"PASS: 20 launcher resources {'verified' if args.check else 'generated'} from {SOURCE.relative_to(ROOT)}.")


if __name__ == "__main__":
    main()
