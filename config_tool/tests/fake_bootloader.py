"""
tests/fake_bootloader.py
========================
An in-memory stand-in for the ATmega2560's stk500boot bootloader.

Mirrors the behaviour the real bootloader's C source has that the client must
respect — in particular that `CMD_PROGRAM_FLASH_ISP` erases pages using its own
cursor rather than the loaded address, so out-of-order writes corrupt flash.
Used to exercise the protocol without hardware.
"""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from stk500v2 import (CMD_ENTER_PROGMODE_ISP, CMD_LEAVE_PROGMODE_ISP,
                      CMD_LOAD_ADDRESS, CMD_PROGRAM_FLASH_ISP,
                      CMD_READ_FLASH_ISP, CMD_READ_SIGNATURE_ISP, CMD_SIGN_ON,
                      FLASH_PAGE_SIZE, MESSAGE_START, STATUS_CMD_OK, TOKEN)

FLASH_SIZE = 256 * 1024
APP_END = FLASH_SIZE - 8 * 1024


class FakeBootloader:
    """Decodes client frames and answers them like stk500boot would."""

    def __init__(self, signature=(0x1E, 0x98, 0x01)) -> None:
        self.flash = bytearray([0xFF] * FLASH_SIZE)
        self.signature = signature
        self.address = 0          # byte address, as the bootloader stores it
        self.erase_address = 0    # independent page-erase cursor
        self.in_progmode = False
        self.erased_pages = []
        self.written_pages = []

    # -- framing ----------------------------------------------------------

    @staticmethod
    def _checksum(frame: bytes) -> int:
        value = 0
        for byte in frame:
            value ^= byte
        return value

    def _frame(self, seq: int, body: bytes) -> bytes:
        header = bytes([MESSAGE_START, seq, (len(body) >> 8) & 0xFF,
                        len(body) & 0xFF, TOKEN])
        return header + body + bytes([self._checksum(header + body)])

    def handle_frame(self, frame: bytes) -> bytes:
        """Take one complete client frame, return the reply frame."""
        assert frame[0] == MESSAGE_START, "bad start byte"
        seq = frame[1]
        size = (frame[2] << 8) | frame[3]
        assert frame[4] == TOKEN, "bad token"
        body = frame[5:5 + size]
        assert frame[5 + size] == self._checksum(frame[:5 + size]), "bad checksum"
        return self._frame(seq, self._dispatch(body))

    # -- commands ---------------------------------------------------------

    def _dispatch(self, body: bytes) -> bytes:
        command = body[0]

        if command == CMD_SIGN_ON:
            return bytes([CMD_SIGN_ON, STATUS_CMD_OK, 8]) + b"AVRISP_2"

        if command == CMD_READ_SIGNATURE_ISP:
            index = body[4]
            return bytes([CMD_READ_SIGNATURE_ISP, STATUS_CMD_OK,
                          self.signature[index], STATUS_CMD_OK])

        if command == CMD_ENTER_PROGMODE_ISP:
            self.in_progmode = True
            return bytes([CMD_ENTER_PROGMODE_ISP, STATUS_CMD_OK])

        if command == CMD_LEAVE_PROGMODE_ISP:
            self.in_progmode = False
            return bytes([CMD_LEAVE_PROGMODE_ISP, STATUS_CMD_OK])

        if command == CMD_LOAD_ADDRESS:
            raw = (body[1] << 24) | (body[2] << 16) | (body[3] << 8) | body[4]
            # The real bootloader shifts left by one into a uint32, which drops
            # avrdude's extended-address flag bit off the top.
            self.address = (raw << 1) & 0xFFFFFFFF
            return bytes([CMD_LOAD_ADDRESS, STATUS_CMD_OK])

        if command == CMD_PROGRAM_FLASH_ISP:
            size = (body[1] << 8) | body[2]
            data = body[10:10 + size]
            assert len(data) == size, "short program payload"
            assert size % 2 == 0 and size > 0, "size must be a non-zero even number"
            if self.erase_address < APP_END:
                self.erased_pages.append(self.erase_address)
                self.flash[self.erase_address:self.erase_address + FLASH_PAGE_SIZE] = \
                    b"\xFF" * FLASH_PAGE_SIZE
                self.erase_address += FLASH_PAGE_SIZE
            self.written_pages.append(self.address)
            self.flash[self.address:self.address + size] = data
            self.address += size
            return bytes([CMD_PROGRAM_FLASH_ISP, STATUS_CMD_OK])

        if command == CMD_READ_FLASH_ISP:
            size = (body[1] << 8) | body[2]
            data = bytes(self.flash[self.address:self.address + size])
            self.address += size
            return bytes([CMD_READ_FLASH_ISP, STATUS_CMD_OK]) + data + \
                bytes([STATUS_CMD_OK])

        raise AssertionError(f"unhandled command 0x{command:02X}")


class FakeSerial:
    """Minimal pyserial stand-in that pipes bytes through a FakeBootloader."""

    def __init__(self, bootloader: FakeBootloader) -> None:
        self.boot = bootloader
        self.is_open = True
        self.dtr = True
        self.rts = True
        self.timeout = 1.0
        self._out = bytearray()     # bytes waiting to be read by the client
        self._in = bytearray()      # partial frame from the client
        self.reset_count = 0

    # -- serial API used by STK500v2 --------------------------------------

    def write(self, data: bytes) -> int:
        self._in.extend(data)
        self._try_dispatch()
        return len(data)

    def read(self, count: int = 1) -> bytes:
        chunk = bytes(self._out[:count])
        del self._out[:count]
        return chunk

    def flush(self) -> None:
        pass

    def reset_input_buffer(self) -> None:
        self._out.clear()

    def reset_output_buffer(self) -> None:
        self._in.clear()

    def close(self) -> None:
        self.is_open = False

    # -- internals ---------------------------------------------------------

    def _try_dispatch(self) -> None:
        """Answer as soon as a complete frame has arrived."""
        while len(self._in) >= 6:
            size = (self._in[2] << 8) | self._in[3]
            total = 5 + size + 1
            if len(self._in) < total:
                return
            frame = bytes(self._in[:total])
            del self._in[:total]
            self._out.extend(self.boot.handle_frame(frame))
