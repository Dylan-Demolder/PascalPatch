"""Render runtime/native/app.ico (PascalPatch.exe and its tray icon) from the app's logo.

    python tooling/native/make_app_icon.py

The logo is host/src/pascalpatch/app/web/icon.svg: a gold parallelogram with an italic P. It is
drawn here at 1024 px and scaled down, so the small sizes stay sharp. Needs Pillow and the
Bahnschrift font that comes with Windows 10 and 11.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "runtime" / "native" / "app.ico"
FONT = Path(r"C:\Windows\Fonts\bahnschrift.ttf")
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
S = 1024


def logo():
    k = S / 64   # the SVG's 64-unit viewBox
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).polygon([(14 * k, 4 * k), (62 * k, 4 * k), (50 * k, 60 * k), (2 * k, 60 * k)], fill=255)
    top, bottom = (0xFF, 0xD7, 0x66), (0xF5, 0xC5, 0x42)
    grad = Image.new("RGB", (1, S))
    for y in range(S):
        t = y / (S - 1)
        grad.putpixel((0, y), tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    img.paste(grad.resize((S, S)), (0, 0), mask)
    # the P: bold, then sheared for the italic
    font = ImageFont.truetype(str(FONT), round(44 * k))
    font.set_variation_by_axes([700, 100]) if hasattr(font, "set_variation_by_axes") else None
    glyph = Image.new("L", (S, S), 0)
    ImageDraw.Draw(glyph).text((33 * k, 48 * k), "P", font=font, fill=255, anchor="ms")
    shear = 0.2
    glyph = glyph.transform((S, S), Image.AFFINE, (1, shear, -shear * 48 * k, 0, 1, 0), Image.BICUBIC)
    img.paste(Image.new("RGBA", (S, S), (0x1A, 0x14, 0x05, 255)), (0, 0), glyph)
    return img


def main():
    big = logo()
    frames = [big.resize((n, n), Image.LANCZOS) for n in SIZES]
    frames[-1].save(OUT, format="ICO", sizes=[(n, n) for n in SIZES], append_images=frames[:-1])
    print(OUT)


if __name__ == "__main__":
    main()
