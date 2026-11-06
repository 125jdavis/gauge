"""
intel_hex.py
============
Minimal Intel HEX reader for AVR firmware images.

Turns a ``.hex`` file exported by the Arduino IDE into a flat byte image
starting at address 0, padded with 0xFF (erased flash) across any gaps.

ATmega2560 images run past 64 KB, so record type 04 (extended linear address)
must be honoured — ignoring it silently folds the upper half of the sketch onto
the lower half.

Usage::

    from intel_hex import parse_hex
    image = parse_hex("gauge_V4.ino.hex")   # bytearray, index == flash address
"""

from __future__ import annotations

# Unwritten flash reads back as 0xFF, so that is what gaps are padded with.
PAD_BYTE = 0xFF

# Intel HEX record types.
_REC_DATA = 0x00
_REC_EOF = 0x01
_REC_EXT_SEGMENT = 0x02
_REC_START_SEGMENT = 0x03
_REC_EXT_LINEAR = 0x04
_REC_START_LINEAR = 0x05


class HexError(ValueError):
    """Raised when a file is not well-formed Intel HEX."""


def _decode_record(line: str, lineno: int) -> bytes:
    """Decode and checksum one ``:LLAAAATT<data>CC`` record."""
    if not line.startswith(":"):
        raise HexError(
            f"Line {lineno}: not Intel HEX (records must start with ':').\n"
            "Make sure this is the .hex file, not a .zip, .elf or renamed file."
        )
    body = line[1:]
    if len(body) % 2 or len(body) < 10:
        raise HexError(f"Line {lineno}: malformed Intel HEX record length.")
    try:
        data = bytes.fromhex(body)
    except ValueError:
        raise HexError(f"Line {lineno}: non-hexadecimal characters in record.") from None
    if data[0] != len(data) - 5:
        raise HexError(f"Line {lineno}: byte-count field does not match record.")
    if (sum(data) & 0xFF) != 0:
        raise HexError(f"Line {lineno}: checksum mismatch — the file is corrupt.")
    return data


def parse_hex(path: str) -> bytearray:
    """Read *path* and return a flat, 0xFF-padded image starting at address 0.

    Args:
        path: Path to an Intel HEX file.

    Returns:
        A bytearray whose index is the flash byte address.

    Raises:
        HexError: if the file is malformed, truncated, or contains no data.
    """
    image = bytearray()
    base = 0           # upper address bits from type 02/04 records
    saw_eof = False

    with open(path, "r", encoding="ascii", errors="replace") as handle:
        for lineno, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            if saw_eof:
                raise HexError(f"Line {lineno}: data after the end-of-file record.")

            record = _decode_record(line, lineno)
            count = record[0]
            offset = (record[1] << 8) | record[2]
            rectype = record[3]
            payload = record[4:4 + count]

            if rectype == _REC_DATA:
                start = base + offset
                end = start + count
                if end > len(image):
                    image.extend([PAD_BYTE] * (end - len(image)))
                image[start:end] = payload
            elif rectype == _REC_EOF:
                saw_eof = True
            elif rectype == _REC_EXT_LINEAR:
                if count != 2:
                    raise HexError(f"Line {lineno}: bad extended linear address record.")
                base = ((payload[0] << 8) | payload[1]) << 16
            elif rectype == _REC_EXT_SEGMENT:
                if count != 2:
                    raise HexError(f"Line {lineno}: bad extended segment address record.")
                base = ((payload[0] << 8) | payload[1]) << 4
            elif rectype in (_REC_START_SEGMENT, _REC_START_LINEAR):
                pass       # entry-point records; the AVR always starts at 0
            else:
                raise HexError(f"Line {lineno}: unsupported record type 0x{rectype:02X}.")

    if not saw_eof:
        raise HexError("File is missing its end-of-file record — it looks truncated.")
    if not image:
        raise HexError("File contains no program data.")
    return image
