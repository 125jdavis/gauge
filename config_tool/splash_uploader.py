"""
splash_uploader.py
==================
High-level splash screen upload helper.

Wraps :class:`serial_protocol.GaugeSerial` and
:class:`ssd1306_converter.SSD1306Converter` to provide a one-call
upload workflow with progress callbacks.

Usage::

    from splash_uploader import SplashUploader
    up = SplashUploader(gauge_serial)
    up.upload(slot=1, path="my_logo.png",
              progress_cb=lambda pct: print(f"{pct:.0f}%"))
"""

from __future__ import annotations

import zlib
from typing import Callable, Optional

from ssd1306_converter import SSD1306Converter
from serial_protocol import GaugeSerial, SerialError

# Bytes per chunk sent in each "splash data" command (max 32 per spec)
CHUNK_SIZE = 32


class SplashUploader:
    """Upload, test, and clear custom splash images."""

    def __init__(self, serial: GaugeSerial) -> None:
        self._serial = serial
        self._converter = SSD1306Converter()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def upload(
        self,
        slot: int,
        path: str,
        progress_cb: Optional[Callable[[float], None]] = None,
    ) -> None:
        """Convert *path* and upload it to *slot* (1 or 2).

        Args:
            slot:        Splash slot number (1 or 2).
            path:        Path to any Pillow-compatible image file.
            progress_cb: Optional callable receiving a float 0.0–100.0 as
                         each chunk is acknowledged.

        Raises:
            SerialError: on protocol errors (including CRC mismatch).
            ValueError: if *slot* is not 1 or 2.
        """
        if slot not in (1, 2):
            raise ValueError(f"slot must be 1 or 2, got {slot}")

        data = self._converter.convert(path)
        self._upload_bytes(slot, data, progress_cb)

    def upload_bytes(
        self,
        slot: int,
        data: bytes,
        progress_cb: Optional[Callable[[float], None]] = None,
    ) -> None:
        """Upload pre-converted 512-byte SSD1306 data directly.

        Args:
            slot:        Splash slot number (1 or 2).
            data:        512 bytes in SSD1306 vertical-byte format.
            progress_cb: Optional progress callback (0.0–100.0).
        """
        if len(data) != 512:
            raise ValueError(f"data must be 512 bytes, got {len(data)}")
        if slot not in (1, 2):
            raise ValueError(f"slot must be 1 or 2, got {slot}")
        self._upload_bytes(slot, data, progress_cb)

    def test(self, slot: int) -> None:
        """Ask the Arduino to display the uploaded splash immediately."""
        self._serial.splash_test(slot)

    def clear(self, slot: int) -> None:
        """Erase *slot* and revert to the built-in default splash."""
        self._serial.splash_clear(slot)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _upload_bytes(
        self,
        slot: int,
        data: bytes,
        progress_cb: Optional[Callable[[float], None]],
    ) -> None:
        crc_value = zlib.crc32(data) & 0xFFFFFFFF
        total_chunks = (len(data) + CHUNK_SIZE - 1) // CHUNK_SIZE

        self._serial.splash_begin(slot)

        for i, offset in enumerate(range(0, len(data), CHUNK_SIZE)):
            chunk = data[offset : offset + CHUNK_SIZE]
            self._serial.splash_data(chunk)
            if progress_cb is not None:
                progress_cb((i + 1) / total_chunks * 100.0)

        self._serial.splash_end(crc_value)
