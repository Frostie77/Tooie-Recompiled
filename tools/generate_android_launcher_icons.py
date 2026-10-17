#!/usr/bin/env python3
"""Regenerate committed Android launcher resources from assets/icon.png.

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
BACKGROUND = "#211D4F"
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
        legacy = centered(source, size, round(size * 0.92), BACKGROUND)
        output[f"mipmap-{density}/ic_launcher.png"] = png(legacy)
        legacy.putalpha(mask(size, "circle"))
        output[f"mipmap-{density}/ic_launcher_round.png"] = png(legacy)
        # The mark fits inside Android's central 66 dp safe circle. Leave the
        # full 108 dp layer transparent outside it; the launcher applies masks.
        foreground = centered(source, round(108 * scale), round(60 * scale))
        output[f"mipmap-{density}/ic_launcher_foreground.png"] = png(foreground)

    for api in (26, 33):
        monochrome = ('    <monochrome android:drawable="@mipmap/ic_launcher_foreground" />\n'
                      if api == 33 else "")
        xml = ('<?xml version="1.0" encoding="utf-8"?>\n'
               '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
               '    <background android:drawable="@color/ic_launcher_background" />\n'
               '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
               + monochrome + '</adaptive-icon>\n')
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
    full = centered(source, 432, 240, BACKGROUND)
    # Adaptive layers are 108 dp; the default mask viewport is the inner 72 dp.
    viewport = full.crop((72, 72, 360, 360)).resize((140, 140), Image.Resampling.LANCZOS)
    for x, shape, label in ((20, "square", "Square"), (200, "circle", "Circle"),
                            (380, "rounded", "Rounded")):
        icon = viewport.copy()
        icon.putalpha(mask(140, shape))
        sheet.paste(icon, (x, 22), icon)
        draw.text((x + 70, 182), label, fill=BACKGROUND, font=font, anchor="mm")
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify without writing")
    parser.add_argument("--preview", type=Path, help="also render example launcher masks")
    args = parser.parse_args()
    with Image.open(ROOT / "assets/icon.png") as image:
        source = image.convert("RGBA")
    if source.size != (128, 128):
        raise SystemExit("Expected the existing 128x128 project icon; review sizing before replacing it.")
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
    print(f"PASS: 20 launcher resources {'verified' if args.check else 'generated'} from assets/icon.png.")


if __name__ == "__main__":
    main()
