"""
ssd1306_converter.py
====================
Convert arbitrary image files to the 128×32 SSD1306 byte-array format
required by the Arduino splash screen upload protocol.

SSD1306 memory layout (CONFIG_TOOL_SPECIFICATION.md §10):
  - 4 pages × 128 columns = 512 bytes
  - Byte at ``page * 128 + col`` encodes 8 vertical pixels (LSB = top row of page)
  - Page 0 → rows 0–7, Page 1 → rows 8–15, Page 2 → rows 16–23, Page 3 → rows 24–31

Usage::

    from ssd1306_converter import SSD1306Converter
    conv = SSD1306Converter()
    data = conv.convert("my_logo.png")   # returns 512-byte bytearray
    preview = conv.preview(data)         # returns PIL.Image 128×32
"""

from __future__ import annotations

from typing import Union

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


class SSD1306Converter:
    """Convert images to SSD1306 128×32 vertical-byte format."""

    WIDTH  = 128
    HEIGHT = 32
    SIZE   = 512  # WIDTH * HEIGHT // 8

    def convert(self, path: str) -> bytes:
        """Load *path*, resize to 128×32, convert to SSD1306 bytes.

        Args:
            path: Path to any PIL-supported image file (PNG, BMP, JPEG …).

        Returns:
            512-byte :class:`bytes` object in SSD1306 vertical-byte format.

        Raises:
            ImportError: if Pillow is not installed.
            FileNotFoundError: if *path* does not exist.
        """
        if not HAS_PIL:
            raise ImportError("Pillow is required: pip install Pillow")
        img = Image.open(path)
        return self.convert_pil(img)

    def convert_pil(self, img: "Image.Image") -> bytes:
        """Convert a PIL Image to SSD1306 bytes.

        The image is converted to monochrome (mode '1') and resized to
        128×32 using LANCZOS resampling before encoding.

        Args:
            img: Any PIL Image object.

        Returns:
            512-byte :class:`bytes` in SSD1306 vertical-byte format.
        """
        if not HAS_PIL:
            raise ImportError("Pillow is required: pip install Pillow")
        img = img.convert("1")  # Force monochrome
        img = img.resize((self.WIDTH, self.HEIGHT), Image.LANCZOS)
        buf = bytearray(self.SIZE)
        for page in range(4):
            for col in range(self.WIDTH):
                byte = 0
                for bit in range(8):
                    row = page * 8 + bit
                    px = img.getpixel((col, row))
                    # In mode '1', non-zero means white
                    if px:
                        byte |= (1 << bit)
                buf[page * self.WIDTH + col] = byte
        return bytes(buf)

    def preview(self, data: bytes) -> "Image.Image":
        """Reconstruct a PIL Image from SSD1306 bytes.

        Useful for displaying a preview of what was uploaded to the Arduino.

        Args:
            data: 512-byte SSD1306-format byte string.

        Returns:
            128×32 PIL Image in mode 'L' (grayscale; white=255, black=0).

        Raises:
            ValueError: if ``len(data) != 512``.
        """
        if not HAS_PIL:
            raise ImportError("Pillow is required: pip install Pillow")
        if len(data) != self.SIZE:
            raise ValueError(f"data must be exactly {self.SIZE} bytes, got {len(data)}")
        img = Image.new("L", (self.WIDTH, self.HEIGHT), color=0)
        for page in range(4):
            for col in range(self.WIDTH):
                byte = data[page * self.WIDTH + col]
                for bit in range(8):
                    row = page * 8 + bit
                    pixel_value = 255 if (byte >> bit) & 1 else 0
                    img.putpixel((col, row), pixel_value)
        return img
