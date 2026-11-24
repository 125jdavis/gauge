"""
ssd1306_converter.py
====================
Convert arbitrary image files to the 512-byte 128×32 monochrome bitmap used
for splash screens.

Byte layout (CONFIG_TOOL_SPECIFICATION.md §10) — the Adafruit GFX
``drawBitmap()`` format, the same as the built-in images in
``gauge_V4/image_data.cpp``:
  - 32 rows × 16 bytes, row 0 first
  - Byte at ``row * 16 + col // 8`` holds 8 horizontal pixels
  - MSB = leftmost pixel; 1 = lit

Usage::

    from ssd1306_converter import SSD1306Converter
    conv = SSD1306Converter()
    data = conv.convert("my_logo.png")   # returns 512 bytes
    preview = conv.preview(data)         # returns PIL.Image 128×32
"""

from __future__ import annotations

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


class SSD1306Converter:
    """Convert images to the 128×32 drawBitmap format used for splash screens."""

    WIDTH  = 128
    HEIGHT = 32
    SIZE   = 512  # WIDTH * HEIGHT // 8
    ROW_BYTES = WIDTH // 8
    THRESHOLD = 128  # grey level at or above which a pixel is lit

    def convert(self, path: str) -> bytes:
        """Load *path*, scale it to fit 128×32 and convert it to bitmap bytes.

        Args:
            path: Path to any PIL-supported image file (PNG, BMP, JPEG …).

        Returns:
            512-byte :class:`bytes` in drawBitmap format.

        Raises:
            ImportError: if Pillow is not installed.
            FileNotFoundError: if *path* does not exist.
        """
        if not HAS_PIL:
            raise ImportError("Pillow is required: pip install Pillow")
        with Image.open(path) as img:
            return self.convert_pil(img)

    def convert_pil(self, img: "Image.Image") -> bytes:
        """Convert a PIL Image to bitmap bytes.

        The image is scaled to fit 128×32 (aspect ratio kept, centred on black),
        then thresholded to 1 bit. Transparent areas count as black.

        Args:
            img: Any PIL Image object.

        Returns:
            512-byte :class:`bytes` in drawBitmap format.
        """
        if not HAS_PIL:
            raise ImportError("Pillow is required: pip install Pillow")
        if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
            rgba = img.convert("RGBA")
            flat = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
            flat.alpha_composite(rgba)
            img = flat
        grey = img.convert("L")
        if grey.size != (self.WIDTH, self.HEIGHT):
            scale = min(self.WIDTH / grey.width, self.HEIGHT / grey.height)
            size = (max(1, round(grey.width * scale)), max(1, round(grey.height * scale)))
            scaled = grey.resize(size, Image.LANCZOS)
            grey = Image.new("L", (self.WIDTH, self.HEIGHT), 0)
            grey.paste(scaled, ((self.WIDTH - size[0]) // 2, (self.HEIGHT - size[1]) // 2))
        buf = bytearray(self.SIZE)
        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                if grey.getpixel((x, y)) >= self.THRESHOLD:
                    buf[y * self.ROW_BYTES + x // 8] |= 0x80 >> (x % 8)
        return bytes(buf)

    def preview(self, data: bytes) -> "Image.Image":
        """Reconstruct a PIL Image from bitmap bytes.

        Args:
            data: 512-byte drawBitmap-format byte string.

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
        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                if data[y * self.ROW_BYTES + x // 8] & (0x80 >> (x % 8)):
                    img.putpixel((x, y), 255)
        return img
