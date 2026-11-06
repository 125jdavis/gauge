"""
stk500v2.py
===========
Pure-Python STK500v2 bootloader client — no avrdude, no Arduino IDE.

Speaks the protocol the ATmega2560's `stk500boot` bootloader implements, so the
configuration tool can flash firmware with nothing installed but pyserial.

Protocol summary (Atmel AVR068 / AVR069):
  Frame:  0x1B <seq> <size hi> <size lo> 0x0E <body...> <xor checksum>
  Wire:   USB serial, 115200 8N1, board is reset by toggling DTR/RTS.
  Body:   <command byte> <command-specific payload>
  Reply:  echoes the command byte, then a status byte (0x00 = OK).

Bootloader behaviour that constrains the caller (from stk500boot.c):
  - `CMD_LOAD_ADDRESS` takes a **word** address and shifts it left by one to get
    a byte address, so bit 31 (avrdude's "extended address" flag) is shifted off
    the top and is harmless to set.
  - `CMD_PROGRAM_FLASH_ISP` erases one page per call, walking an internal
    `eraseAddress` cursor forward by SPM_PAGESIZE independently of the address
    that was loaded. That cursor starts at 0 when the bootloader starts, so
    **pages must be written in order from address 0** or erase and write
    desynchronise and the image is corrupted.
  - The mode/delay/cmd/poll header bytes of `CMD_PROGRAM_FLASH_ISP` are ignored
    by the bootloader (it uses boot_page_*), but are sent for compatibility.

Usage::

    with STK500v2("COM3") as boot:
        boot.enter_progmode()
        boot.program_flash(image, progress_cb=print)
        boot.verify_flash(image)
        boot.leave_progmode()
"""

from __future__ import annotations

import time
from typing import Callable, Optional

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# --- Framing ---------------------------------------------------------------
MESSAGE_START = 0x1B
TOKEN = 0x0E

# --- Commands --------------------------------------------------------------
CMD_SIGN_ON = 0x01
CMD_LOAD_ADDRESS = 0x06
CMD_ENTER_PROGMODE_ISP = 0x10
CMD_LEAVE_PROGMODE_ISP = 0x11
CMD_PROGRAM_FLASH_ISP = 0x13
CMD_READ_FLASH_ISP = 0x14
CMD_READ_SIGNATURE_ISP = 0x1B

STATUS_CMD_OK = 0x00

# --- Device -----------------------------------------------------------------
# ATmega2560: 256 KB flash in 256-byte pages; the last 8 KB holds the bootloader.
FLASH_PAGE_SIZE = 256
ATMEGA2560_SIGNATURE = (0x1E, 0x98, 0x01)
BOOTLOADER_BAUD = 115200

# Known AVR signatures, so a mismatch can name what was actually found.
SIGNATURES = {
    (0x1E, 0x98, 0x01): "ATmega2560",
    (0x1E, 0x98, 0x03): "ATmega2561",
    (0x1E, 0x97, 0x03): "ATmega1280",
    (0x1E, 0x97, 0x04): "ATmega1281",
    (0x1E, 0x95, 0x0F): "ATmega328P",
    (0x1E, 0x95, 0x87): "ATmega32U4",
}

# The bootloader answers within milliseconds; a slow reply means it is not there.
READ_TIMEOUT = 1.0
SYNC_READ_TIMEOUT = 0.1
SYNC_ATTEMPTS = 40


class STK500Error(RuntimeError):
    """Raised on a protocol, timeout, or verification failure."""


class STK500v2:
    """STK500v2 bootloader session over a serial port."""

    def __init__(self, port: str, baud: int = BOOTLOADER_BAUD) -> None:
        if not HAS_SERIAL:
            raise STK500Error("pyserial is not installed. Run: pip install pyserial")
        self._port_name = port
        self._baud = baud
        self._ser: Optional["serial.Serial"] = None
        self._seq = 0

    # ------------------------------------------------------------------
    # Session management
    # ------------------------------------------------------------------

    def __enter__(self) -> "STK500v2":
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def open(self) -> None:
        """Open the port, reset the board into its bootloader, and sync."""
        try:
            self._ser = serial.Serial(self._port_name, self._baud, timeout=READ_TIMEOUT)
        except Exception as exc:
            raise STK500Error(
                f"Could not open {self._port_name}: {exc}\n"
                "Close any other program using the port, then retry."
            ) from exc
        self._reset_board()
        self._sync()

    def close(self) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    def _reset_board(self) -> None:
        """Pulse DTR/RTS to reset the board, the same way the Arduino IDE does.

        Dropping both lines discharges the auto-reset capacitor; raising them
        again pulls RESET low briefly, so the bootloader starts fresh.
        """
        assert self._ser is not None
        self._ser.dtr = False
        self._ser.rts = False
        time.sleep(0.25)
        self._ser.dtr = True
        self._ser.rts = True
        time.sleep(0.05)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()

    def _sync(self) -> None:
        """Handshake with the bootloader, retrying while it finishes starting.

        The bootloader only listens briefly after a reset and drops frames sent
        before it is up, so poll with short timeouts instead of one long wait.
        """
        assert self._ser is not None
        last_error = "no response"
        normal_timeout = self._ser.timeout
        self._ser.timeout = SYNC_READ_TIMEOUT
        try:
            for _ in range(SYNC_ATTEMPTS):
                self._seq = 0  # every sign-on uses sequence 1, so late replies still match
                try:
                    reply = self._transact(bytes([CMD_SIGN_ON]))
                except STK500Error as exc:
                    last_error = str(exc)
                    self._ser.reset_input_buffer()
                    continue
                if len(reply) >= 3 and reply[1] == STATUS_CMD_OK:
                    # Drop duplicate sign-on replies from the extra attempts.
                    time.sleep(0.1)
                    self._ser.reset_input_buffer()
                    return
                last_error = "unexpected sign-on reply"
        finally:
            self._ser.timeout = normal_timeout
        raise STK500Error(
            f"The bootloader did not respond on {self._port_name} ({last_error}).\n"
            "Check the USB cable, confirm the port, and make sure no other program "
            "(serial monitor, this tool's own connection) is using it."
        )

    # ------------------------------------------------------------------
    # Framing
    # ------------------------------------------------------------------

    def _transact(self, body: bytes) -> bytes:
        """Send one command frame and return the reply body."""
        self._send_frame(body)
        reply = self._read_frame()
        if not reply:
            raise STK500Error("Empty reply from bootloader.")
        if reply[0] != body[0]:
            raise STK500Error(
                f"Bootloader replied to command 0x{reply[0]:02X}, expected 0x{body[0]:02X}."
            )
        return reply

    def _expect_ok(self, body: bytes, what: str) -> bytes:
        reply = self._transact(body)
        if len(reply) < 2 or reply[1] != STATUS_CMD_OK:
            status = reply[1] if len(reply) > 1 else -1
            raise STK500Error(f"{what} failed (bootloader status 0x{status:02X}).")
        return reply

    def _send_frame(self, body: bytes) -> None:
        assert self._ser is not None
        self._seq = (self._seq + 1) & 0xFF
        header = bytes([MESSAGE_START, self._seq,
                        (len(body) >> 8) & 0xFF, len(body) & 0xFF, TOKEN])
        frame = header + body
        checksum = 0
        for byte in frame:
            checksum ^= byte
        self._ser.write(frame + bytes([checksum]))
        self._ser.flush()

    def _read_exact(self, count: int) -> bytes:
        assert self._ser is not None
        data = self._ser.read(count)
        if len(data) != count:
            raise STK500Error("Timed out waiting for the bootloader.")
        return data

    def _read_frame(self) -> bytes:
        """Read one reply frame, resynchronising on the start byte."""
        assert self._ser is not None
        deadline = time.monotonic() + READ_TIMEOUT * 2

        # Skip any startup chatter until a frame actually begins.
        while True:
            start = self._ser.read(1)
            if not start:
                raise STK500Error("Timed out waiting for the bootloader.")
            if start[0] == MESSAGE_START:
                break
            if time.monotonic() > deadline:
                raise STK500Error("Could not find the start of a reply frame.")

        seq = self._read_exact(1)[0]
        size_bytes = self._read_exact(2)
        size = (size_bytes[0] << 8) | size_bytes[1]
        token = self._read_exact(1)[0]
        if token != TOKEN:
            raise STK500Error(f"Bad frame token 0x{token:02X}.")
        body = self._read_exact(size)
        checksum = self._read_exact(1)[0]

        expected = MESSAGE_START ^ seq ^ size_bytes[0] ^ size_bytes[1] ^ TOKEN
        for byte in body:
            expected ^= byte
        if checksum != expected:
            raise STK500Error("Reply checksum mismatch — serial link problem.")
        if seq != self._seq:
            raise STK500Error(f"Out-of-sequence reply ({seq} != {self._seq}).")
        return body

    # ------------------------------------------------------------------
    # Commands
    # ------------------------------------------------------------------

    def read_signature(self) -> tuple:
        """Return the 3-byte device signature as a tuple."""
        sig = []
        for index in range(3):
            # Payload mirrors the ISP read-signature instruction; the bootloader
            # only uses the index byte.
            reply = self._expect_ok(
                bytes([CMD_READ_SIGNATURE_ISP, 0x00, 0x30, 0x00, index, 0x00]),
                "Reading the device signature",
            )
            if len(reply) < 3:
                raise STK500Error("Short signature reply from bootloader.")
            sig.append(reply[2])
        return tuple(sig)

    def enter_progmode(self) -> None:
        """Enter programming mode. Resets the bootloader's page-erase cursor."""
        self._expect_ok(
            bytes([CMD_ENTER_PROGMODE_ISP,
                   200,        # timeout (ms)
                   100,        # stabDelay
                   25,         # cmdexeDelay
                   32,         # synchLoops
                   0,          # byteDelay
                   0x53,       # pollValue
                   3,          # pollIndex
                   0xAC, 0x53, 0x00, 0x00]),
            "Entering programming mode",
        )

    def leave_progmode(self) -> None:
        """Leave programming mode so the bootloader starts the new sketch."""
        self._expect_ok(bytes([CMD_LEAVE_PROGMODE_ISP, 1, 1]),
                        "Leaving programming mode")

    def load_address(self, byte_address: int) -> None:
        """Point the bootloader at *byte_address*.

        The wire value is a word address with bit 31 set: the bootloader shifts
        left by one to recover the byte address, which discards that flag bit.
        """
        word = (byte_address >> 1) | 0x80000000
        self._expect_ok(
            bytes([CMD_LOAD_ADDRESS,
                   (word >> 24) & 0xFF, (word >> 16) & 0xFF,
                   (word >> 8) & 0xFF, word & 0xFF]),
            f"Setting the flash address to 0x{byte_address:05X}",
        )

    def _program_page(self, page: bytes) -> None:
        """Write one already-addressed flash page."""
        header = bytes([CMD_PROGRAM_FLASH_ISP,
                        (len(page) >> 8) & 0xFF, len(page) & 0xFF,
                        0xC1,       # mode  ) ignored by the bootloader, sent
                        0x06,       # delay ) for avrdude compatibility
                        0x40, 0x4C, 0x20,   # cmd1..3
                        0x00, 0x00])        # poll1..2
        self._expect_ok(header + page, "Writing a flash page")

    def _read_page(self, length: int) -> bytes:
        reply = self._expect_ok(
            bytes([CMD_READ_FLASH_ISP, (length >> 8) & 0xFF, length & 0xFF, 0x20]),
            "Reading flash back",
        )
        # Reply is: cmd, STATUS_CMD_OK, <data...>, STATUS_CMD_OK
        data = reply[2:2 + length]
        if len(data) != length:
            raise STK500Error("Short flash read from bootloader.")
        return bytes(data)

    # ------------------------------------------------------------------
    # High-level flash operations
    # ------------------------------------------------------------------

    def program_flash(self, image: bytes,
                      progress_cb: Optional[Callable[[float], None]] = None) -> None:
        """Write *image* to flash, page by page from address 0.

        Writing strictly in order and starting at 0 is required: the bootloader
        erases one page per write using its own cursor, not the loaded address.

        Args:
            image:       Flat firmware image; index is the flash byte address.
            progress_cb: Optional callback receiving 0.0-100.0.
        """
        padded = self._pad_to_page(image)
        total = len(padded) // FLASH_PAGE_SIZE
        for index in range(total):
            offset = index * FLASH_PAGE_SIZE
            self.load_address(offset)
            self._program_page(padded[offset:offset + FLASH_PAGE_SIZE])
            if progress_cb:
                progress_cb((index + 1) / total * 100.0)

    def verify_flash(self, image: bytes,
                     progress_cb: Optional[Callable[[float], None]] = None) -> None:
        """Read flash back and compare it against *image*.

        Raises:
            STK500Error: naming the first address that differs.
        """
        padded = self._pad_to_page(image)
        total = len(padded) // FLASH_PAGE_SIZE
        for index in range(total):
            offset = index * FLASH_PAGE_SIZE
            self.load_address(offset)
            actual = self._read_page(FLASH_PAGE_SIZE)
            expected = padded[offset:offset + FLASH_PAGE_SIZE]
            if actual != expected:
                for position, (got, want) in enumerate(zip(actual, expected)):
                    if got != want:
                        raise STK500Error(
                            f"Verification failed at 0x{offset + position:05X}: "
                            f"flash holds 0x{got:02X}, expected 0x{want:02X}."
                        )
            if progress_cb:
                progress_cb((index + 1) / total * 100.0)

    @staticmethod
    def _pad_to_page(image: bytes) -> bytes:
        """Round *image* up to a whole number of flash pages with 0xFF."""
        remainder = len(image) % FLASH_PAGE_SIZE
        if remainder:
            image = bytes(image) + b"\xFF" * (FLASH_PAGE_SIZE - remainder)
        return bytes(image)
