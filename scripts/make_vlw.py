#!/usr/bin/env python3
"""Generate LovyanGFX/TFT_eSPI smooth fonts (.vlw) from a TTF with Pillow.

VLW layout (all ints big-endian int32):
  header : glyph_count, version(11), font_size, 0, ascent, descent
  glyphs : unicode, height, width, x_advance, dY (top above baseline), dX, 0
  bitmaps: width*height 8-bit alpha per glyph, same order as the records

Usage:
  make_vlw.py FONT.ttf SIZE OUT.vlw [--chars latin1|digits|"literal"]
"""

import argparse
import struct

from PIL import Image, ImageDraw, ImageFont

LATIN1 = [c for c in range(33, 127)] + [c for c in range(161, 256)] + [0x2014, 0x2022, 0x2026]
DIGITS = [ord(c) for c in "0123456789-+:.,%°C"]


def build(ttf: str, size: int, codepoints: list[int]) -> bytes:
    font = ImageFont.truetype(ttf, size)
    records = []
    bitmaps = []
    for cp in sorted(set(codepoints)):
        ch = chr(cp)
        adv = int(round(font.getlength(ch)))
        # Render on a roomy canvas with the baseline origin at (ox, oy), then
        # crop to the ink so records carry tight boxes (Pillow's getbbox is loose).
        ox, oy = size, size * 2
        canvas = Image.new("L", (size * 4, size * 3), 0)
        ImageDraw.Draw(canvas).text((ox, oy), ch, font=font, fill=255, anchor="ls")
        ink = canvas.getbbox()
        if ink is None:
            continue
        x0, y0, x1, y1 = ink
        img = canvas.crop(ink)
        records.append((cp, y1 - y0, x1 - x0, adv, oy - y0, x0 - ox, 0))
        bitmaps.append(img.tobytes())

    def rec(cp):
        return next(r for r in records if r[0] == cp)

    # Match Processing's convention: ascent from 'd', descent from 'p'.
    have = {r[0] for r in records}
    ascent = rec(ord("d"))[4] if ord("d") in have else max(r[4] for r in records)
    descent = (rec(ord("p"))[1] - rec(ord("p"))[4]) if ord("p") in have else 0

    out = struct.pack(">6i", len(records), 11, size, 0, ascent, descent)
    for r in records:
        out += struct.pack(">7i", *r)
    for b in bitmaps:
        out += b
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ttf")
    ap.add_argument("size", type=int)
    ap.add_argument("out")
    ap.add_argument("--chars", default="latin1")
    a = ap.parse_args()
    if a.chars == "latin1":
        cps = LATIN1
    elif a.chars == "digits":
        cps = DIGITS
    else:
        cps = [ord(c) for c in a.chars]
    data = build(a.ttf, a.size, cps)
    with open(a.out, "wb") as f:
        f.write(data)
    print(f"{a.out}: {len(data)} bytes, {struct.unpack('>i', data[:4])[0]} glyphs")


if __name__ == "__main__":
    main()
