"""
make_icon.py
============
Regenerate ``gauge.ico`` (window, taskbar and .exe icon) and ``gauge_icon.png``.

    python make_icon.py

The large frames (32 px and up, used by the taskbar, Alt-Tab and Explorer) are the
full gauge from ``gauge_icon.svg`` with its dial enlarged to fill the icon. The small frames (16-24 px, which Windows uses
for the window's title bar) are a simplified version: a dial with a single tick ring,
the orange redline arc and the needle, so it stays readable at that size.

Drawn with Pillow (already a dependency), supersampled and downsized for clean edges.
"""

from __future__ import annotations

import math
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ICO_PATH = os.path.join(HERE, "gauge.ico")
PNG_PATH = os.path.join(HERE, "gauge_icon.png")

DIAL = (26, 32, 41, 255)
TICK = (232, 236, 241, 255)
ORANGE = (255, 106, 26, 255)

# Tick marks from gauge_icon.svg (512 x 512 canvas): (x1, y1, x2, y2, redline?)
TICKS = [
    (124.5, 387.5, 151.3, 360.7, False), (97.4, 353.2, 129.8, 333.3, False),
    (79.1, 313.5, 115.2, 301.7, False), (70.6, 270.6, 108.5, 267.6, False),
    (72.3, 226.9, 109.8, 232.8, False), (84.2, 184.8, 119.3, 199.4, False),
    (105.5, 146.7, 136.3, 169.0, False), (135.2, 114.6, 159.9, 143.5, False),
    (171.6, 90.3, 188.8, 124.1, False), (212.6, 75.1, 221.5, 112.1, False),
    (256.0, 70.0, 256.0, 108.0, False), (299.4, 75.1, 290.5, 112.1, False),
    (340.4, 90.3, 323.2, 124.1, False), (376.8, 114.6, 352.1, 143.5, False),
    (406.5, 146.7, 375.7, 169.0, False), (427.8, 184.8, 392.7, 199.4, False),
    (439.7, 226.9, 402.2, 232.8, True), (441.4, 270.6, 403.5, 267.6, True),
    (432.9, 313.5, 396.8, 301.7, True), (414.6, 353.2, 382.2, 333.3, True),
    (387.5, 387.5, 360.7, 360.7, True),
]
NEEDLE = (233.5, 269.0, 385.9, 181.0)

LARGE_SIZES = (32, 48, 64, 128, 256)
SMALL_SIZES = (16, 20, 24)
SUPERSAMPLE = 4
FULL_SCALE = 250 / 202  # dial radius in gauge_icon.svg -> radius in the icon


def _line(draw: ImageDraw.ImageDraw, s: float, x1, y1, x2, y2, width, color) -> None:
    """A line with round caps, in 512-unit coordinates scaled by ``s``."""
    w = width * s
    draw.line([(x1 * s, y1 * s), (x2 * s, y2 * s)], fill=color, width=max(1, round(w)))
    r = w / 2
    for x, y in ((x1, y1), (x2, y2)):
        draw.ellipse([x * s - r, y * s - r, x * s + r, y * s + r], fill=color)


def _circle(draw, s, cx, cy, r, fill=None, outline=None, width=0) -> None:
    draw.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s],
                 fill=fill, outline=outline, width=max(0, round(width * s)))


def draw_full(size: int) -> Image.Image:
    """The full icon, matching gauge_icon.svg: no background square, and the dial
    scaled up from r=202 to nearly fill the canvas (r=250 of 256)."""
    px = size * SUPERSAMPLE
    # Draw on a larger canvas, then crop the centre, so the dial fills the icon.
    big = round(px * FULL_SCALE)
    s = big / 512
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    _circle(d, s, 256, 256, 202, fill=DIAL)
    for x1, y1, x2, y2, red in TICKS:
        _line(d, s, x1, y1, x2, y2, 10, ORANGE if red else TICK)
    _line(d, s, *NEEDLE, 15, ORANGE)
    # Hub: stroke centred on r=24 with width 12 -> ring from r=18 to r=30.
    _circle(d, s, 256, 256, 30, fill=ORANGE)
    _circle(d, s, 256, 256, 18, fill=DIAL)
    off = (big - px) // 2
    img = img.crop((off, off, off + px, off + px))
    return img.resize((size, size), Image.LANCZOS)


def draw_simple(size: int) -> Image.Image:
    """Simplified title-bar icon: dial, tick ring, redline arc and a bold needle."""
    px = size * SUPERSAMPLE
    s = px / 512
    img = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Dial fills the square (transparent corners so it sits cleanly on any title bar).
    _circle(d, s, 256, 256, 250, fill=DIAL)
    # One continuous tick ring instead of 21 ticks; the gauge sweeps 270 degrees
    # (from 135 to 405 in Pillow's clockwise-from-east angles), redline is the last 60.
    ring_r, ring_w = 186, 52
    box = [(256 - ring_r) * s, (256 - ring_r) * s, (256 + ring_r) * s, (256 + ring_r) * s]
    d.arc(box, 135, 345, fill=TICK, width=round(ring_w * s))
    d.arc(box, 345, 405, fill=ORANGE, width=round(ring_w * s))
    # Needle pointing up-right, same angle as the full icon, but thicker.
    ang = math.atan2(NEEDLE[3] - NEEDLE[1], NEEDLE[2] - NEEDLE[0])
    length = 120  # stops short of the ring so the two read separately
    _line(d, s, 256, 256, 256 + length * math.cos(ang), 256 + length * math.sin(ang),
          60, ORANGE)
    _circle(d, s, 256, 256, 62, fill=ORANGE)
    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    frames = [draw_simple(n) for n in SMALL_SIZES] + [draw_full(n) for n in LARGE_SIZES]
    largest = frames[-1]
    largest.save(
        ICO_PATH, format="ICO",
        sizes=[(n, n) for n in SMALL_SIZES + LARGE_SIZES],
        append_images=frames[:-1],
    )
    largest.save(PNG_PATH)
    print("Wrote", ICO_PATH)
    print("Wrote", PNG_PATH)


if __name__ == "__main__":
    main()
