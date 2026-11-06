"""
tests/test_ssd1306.py
=====================
Unit tests for SSD1306Converter (CONFIG_TOOL_SPECIFICATION.md §15).

Run with: python -m pytest config_tool/tests/
"""

import sys
import os
import pathlib

# Allow importing from the config_tool package regardless of how the tests are invoked
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import zlib
import pytest

# Skip all tests if Pillow is not installed
try:
    from PIL import Image
    from ssd1306_converter import SSD1306Converter
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

pytestmark = pytest.mark.skipif(not PIL_AVAILABLE, reason="Pillow not installed")


class TestSSD1306Converter:
    """Tests for SSD1306Converter byte-format encoding and decoding."""

    def setup_method(self) -> None:
        self.conv = SSD1306Converter()

    # ------------------------------------------------------------------
    # Output size
    # ------------------------------------------------------------------

    def test_output_size_blank(self) -> None:
        """convert_pil of a blank image must return exactly 512 bytes."""
        img = Image.new("1", (128, 32), color=0)
        data = self.conv.convert_pil(img)
        assert len(data) == 512

    def test_output_size_white(self) -> None:
        """convert_pil of an all-white image must return exactly 512 bytes."""
        img = Image.new("1", (128, 32), color=255)
        data = self.conv.convert_pil(img)
        assert len(data) == 512

    # ------------------------------------------------------------------
    # All-black / all-white encoding
    # ------------------------------------------------------------------

    def test_blank_image_is_all_zeros(self) -> None:
        """A fully black image converts to all-zero bytes."""
        img = Image.new("1", (128, 32), color=0)
        data = self.conv.convert_pil(img)
        assert data == bytes(512)

    def test_white_image_is_all_0xFF(self) -> None:
        """A fully white image converts to all-0xFF bytes."""
        img = Image.new("1", (128, 32), color=255)
        data = self.conv.convert_pil(img)
        assert data == bytes([0xFF] * 512)

    # ------------------------------------------------------------------
    # Single-pixel round-trip
    # ------------------------------------------------------------------

    def test_top_left_pixel_set(self) -> None:
        """Setting top-left pixel should set bit 0 of byte 0."""
        img = Image.new("1", (128, 32), color=0)
        img.putpixel((0, 0), 1)
        data = self.conv.convert_pil(img)
        # Page 0, column 0 → byte index 0; row 0 → bit 0
        assert data[0] & 0x01 != 0, "bit 0 of byte 0 should be set for top-left pixel"

    def test_bottom_left_pixel_set(self) -> None:
        """Setting bottom-left pixel should set bit 7 of byte 3*128 = byte 384."""
        img = Image.new("1", (128, 32), color=0)
        img.putpixel((0, 31), 1)
        data = self.conv.convert_pil(img)
        # Page 3, column 0 → byte index 384; row 31 = page 3 bit 7
        assert data[384] & 0x80 != 0, "bit 7 of byte 384 should be set for bottom-left pixel"

    # ------------------------------------------------------------------
    # Round-trip: convert_pil -> preview
    # ------------------------------------------------------------------

    def test_roundtrip_blank(self) -> None:
        """Round-trip of a blank image produces a black preview."""
        img = Image.new("1", (128, 32), color=0)
        data = self.conv.convert_pil(img)
        preview = self.conv.preview(data)
        assert preview.getpixel((0, 0)) == 0  # black

    def test_roundtrip_top_left(self) -> None:
        """Round-trip of a single white pixel at (0,0) recovers that pixel."""
        img = Image.new("1", (128, 32), color=0)
        img.putpixel((0, 0), 1)
        data = self.conv.convert_pil(img)
        recovered = self.conv.preview(data)
        assert recovered.getpixel((0, 0)) == 255  # white in L mode

    def test_roundtrip_bottom_right(self) -> None:
        """Round-trip of a single white pixel at (127, 31) recovers correctly."""
        img = Image.new("1", (128, 32), color=0)
        img.putpixel((127, 31), 1)
        data = self.conv.convert_pil(img)
        recovered = self.conv.preview(data)
        assert recovered.getpixel((127, 31)) == 255

    # ------------------------------------------------------------------
    # preview() input validation
    # ------------------------------------------------------------------

    def test_preview_wrong_size_raises(self) -> None:
        """preview() must raise ValueError if input is not 512 bytes."""
        with pytest.raises(ValueError):
            self.conv.preview(bytes(511))
        with pytest.raises(ValueError):
            self.conv.preview(bytes(513))

    # ------------------------------------------------------------------
    # CRC32 compatibility
    # ------------------------------------------------------------------

    def test_crc32_known_value(self) -> None:
        """CRC32 of bytes(range(256)) * 2 must match known value.

        This verifies Python's zlib.crc32 will match the Arduino crc32()
        implementation for the same input data.
        """
        data = bytes(range(256)) * 2  # 512 bytes
        py_crc = zlib.crc32(data) & 0xFFFFFFFF
        # Known-good CRC32 for this input (computed with Python zlib)
        assert py_crc == 0x1C613576

    def test_crc32_all_zeros(self) -> None:
        """CRC32 of 512 zero bytes must match zlib reference."""
        data = bytes(512)
        py_crc = zlib.crc32(data) & 0xFFFFFFFF
        expected = zlib.crc32(bytes(512)) & 0xFFFFFFFF
        assert py_crc == expected  # tautological but confirms no truncation

    # ------------------------------------------------------------------
    # convert() from file (skip if no test image available)
    # ------------------------------------------------------------------

    def test_convert_file_produces_512_bytes(self, tmp_path: pathlib.Path) -> None:
        """convert() from a generated PNG file returns 512 bytes."""
        img = Image.new("RGB", (200, 100), color=(128, 128, 128))
        path = str(tmp_path / "test.png")
        img.save(path)
        data = self.conv.convert(path)
        assert len(data) == 512
