"""
firmware_uploader.py
====================
Flash a pre-compiled firmware image to the gauge, with nothing else installed.

Implements CONFIG_TOOL_SPECIFICATION.md §13 (Firmware Upload Procedure).

Customers receive a ``.hex`` built here and never run the Arduino IDE, so the
upload talks to the ATmega2560 bootloader directly over USB serial — see
:mod:`stk500v2`. There is no avrdude, no toolchain, and nothing to install
beyond pyserial, which the tool already needs.

Usage::

    from firmware_uploader import FirmwareUploader, find_firmware

    candidate = find_firmware()                  # a .hex shipped next to the app
    uploader = FirmwareUploader()
    uploader.validate(candidate)                 # returns a list of warnings
    uploader.upload(candidate, "COM3", log_cb=print, progress_cb=lambda p: None)
"""

from __future__ import annotations

import glob
import os
import sys
from typing import Callable, List, Optional

from intel_hex import HexError, parse_hex
from stk500v2 import (ATMEGA2560_SIGNATURE, SIGNATURES, STK500Error, STK500v2,
                      BOOTLOADER_BAUD)

# ATmega2560: 256 KB flash, the top 8 KB reserved for the bootloader.
FLASH_TOTAL_BYTES = 256 * 1024
BOOTLOADER_BYTES = 8 * 1024
APP_MAX_BYTES = FLASH_TOTAL_BYTES - BOOTLOADER_BYTES     # 253952

# Warn once the sketch fills most of the application area.
FLASH_WARN_BYTES = int(APP_MAX_BYTES * 0.90)

# Folders searched for a firmware image shipped alongside the tool.
FIRMWARE_SUBDIRS = ("firmware", "Firmware")


class FirmwareError(RuntimeError):
    """Raised when a firmware file is unusable or an upload fails."""


# ----------------------------------------------------------------------
# Locating the firmware image
# ----------------------------------------------------------------------

def app_dir() -> str:
    """Return the folder the tool runs from.

    For a PyInstaller build this is the folder holding the .exe — not the
    temporary extraction directory — so a customer can drop a newer .hex beside
    the executable without it being rebuilt.
    """
    if getattr(sys, "frozen", False):
        return os.path.dirname(os.path.abspath(sys.executable))
    return os.path.dirname(os.path.abspath(__file__))


def firmware_search_dirs() -> List[str]:
    """Return the folders searched for a bundled firmware image, in order."""
    base = app_dir()
    dirs = [os.path.join(base, sub) for sub in FIRMWARE_SUBDIRS]
    dirs.append(base)
    # Convenience when running from a source checkout: the Arduino IDE's
    # "Export Compiled Binary" output folder.
    dirs.append(os.path.normpath(os.path.join(base, "..", "gauge_V4", "build")))
    return [d for d in dirs if os.path.isdir(d)]


def list_firmware() -> List[str]:
    """Return every candidate ``.hex`` found near the tool, newest first."""
    found: List[str] = []
    seen = set()
    for directory in firmware_search_dirs():
        for path in glob.glob(os.path.join(directory, "**", "*.hex"), recursive=True):
            # A with_bootloader image is never the one to flash over serial.
            if "with_bootloader" in os.path.basename(path).lower():
                continue
            if not os.path.isfile(path):
                continue
            # The search folders overlap, and on Windows "firmware" and
            # "Firmware" are the same folder — compare canonical paths so one
            # file is not offered twice.
            key = os.path.normcase(os.path.realpath(path))
            if key in seen:
                continue
            seen.add(key)
            found.append(path)
    found.sort(key=os.path.getmtime, reverse=True)
    return found


def find_firmware() -> Optional[str]:
    """Return the single best bundled firmware image, or None if unclear."""
    candidates = list_firmware()
    return candidates[0] if candidates else None


# ----------------------------------------------------------------------
# Validation
# ----------------------------------------------------------------------

def load_image(path: str) -> bytearray:
    """Parse *path* into a flat flash image.

    Raises:
        FirmwareError: if the file is missing or not valid Intel HEX.
    """
    if not path:
        raise FirmwareError("No firmware file selected.")
    if not os.path.isfile(path):
        raise FirmwareError(f"Firmware file not found:\n{path}")
    if os.path.getsize(path) == 0:
        raise FirmwareError("Firmware file is empty.")
    if os.path.splitext(path)[1].lower() != ".hex":
        raise FirmwareError(
            "Firmware must be a .hex file. Select the file supplied with your gauge."
        )
    try:
        return parse_hex(path)
    except HexError as exc:
        raise FirmwareError(str(exc)) from exc
    except OSError as exc:
        raise FirmwareError(f"Could not read the firmware file:\n{exc}") from exc


def validate(path: str) -> List[str]:
    """Check *path* is a flashable image and return non-fatal warnings.

    Raises:
        FirmwareError: if the file cannot safely be flashed.
    """
    image = load_image(path)
    if len(image) > APP_MAX_BYTES:
        raise FirmwareError(
            f"This image needs {len(image) / 1024:.0f} KB but only "
            f"{APP_MAX_BYTES / 1024:.0f} KB is available — the rest of the flash holds "
            "the bootloader.\nIf this is a 'with_bootloader' file, use the plain .hex instead."
        )
    warnings: List[str] = []
    if len(image) > FLASH_WARN_BYTES:
        warnings.append(
            f"Image is {len(image) / 1024:.0f} KB of the "
            f"{APP_MAX_BYTES / 1024:.0f} KB available."
        )
    return warnings


# ----------------------------------------------------------------------
# Upload
# ----------------------------------------------------------------------

class FirmwareUploader:
    """Flash pre-compiled firmware over the board's bootloader."""

    def __init__(self, baud: int = BOOTLOADER_BAUD) -> None:
        self._baud = baud

    @staticmethod
    def validate(path: str) -> List[str]:
        """Convenience passthrough to the module-level :func:`validate`."""
        return validate(path)

    def upload(
        self,
        path: str,
        port: str,
        log_cb: Optional[Callable[[str], None]] = None,
        progress_cb: Optional[Callable[[float], None]] = None,
        verify: bool = True,
    ) -> None:
        """Flash *path* to the board on *port*.

        Blocks for the duration, so a UI should call this on a worker thread.
        Progress runs 0-100 across the write and, when *verify* is set, the
        read-back pass.

        Args:
            path:        Firmware image (.hex).
            port:        Serial port device name, e.g. ``COM3``.
            log_cb:      Optional progress-message callback.
            progress_cb: Optional 0.0-100.0 callback.
            verify:      Read the flash back and compare after writing.

        Raises:
            FirmwareError: on a bad file, a port problem, or a failed verify.
        """
        def log(message: str) -> None:
            if log_cb:
                log_cb(message)

        image = load_image(path)
        validate(path)
        log(f"{os.path.basename(path)}: {len(image) / 1024:.1f} KB to write.")

        # Writing is roughly twice the work of verifying, but reserving half the
        # bar for each keeps it moving smoothly enough.
        write_share = 50.0 if verify else 100.0

        try:
            with STK500v2(port, self._baud) as boot:
                log(f"Bootloader on {port} responded.")

                signature = boot.read_signature()
                name = SIGNATURES.get(signature)
                if signature != ATMEGA2560_SIGNATURE:
                    found = name or (
                        "unknown device "
                        + " ".join(f"0x{b:02X}" for b in signature)
                    )
                    raise FirmwareError(
                        f"This firmware is for the ATmega2560, but the board reports "
                        f"{found}.\nCheck that the correct device is connected."
                    )
                log(f"Board identified: {name}.")

                boot.enter_progmode()
                log("Writing flash...")
                boot.program_flash(
                    image,
                    progress_cb=lambda pct: progress_cb(pct * write_share / 100.0)
                    if progress_cb else None,
                )

                if verify:
                    log("Verifying...")
                    boot.verify_flash(
                        image,
                        progress_cb=lambda pct: progress_cb(50.0 + pct / 2.0)
                        if progress_cb else None,
                    )
                    log("Verified: flash matches the firmware file.")

                boot.leave_progmode()
        except STK500Error as exc:
            raise FirmwareError(str(exc)) from exc

        if progress_cb:
            progress_cb(100.0)
        log("Upload complete.")
