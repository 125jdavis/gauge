"""
serial_protocol.py
==================
Low-level serial communication layer for the Gauge V4 configuration tool.

Implements the human-readable text protocol described in
CONFIG_TOOL_SPECIFICATION.md §3.

Usage example::

    from serial_protocol import GaugeSerial
    g = GaugeSerial()
    g.connect("COM3")            # or "/dev/ttyUSB0"
    print(g.ping())              # True
    print(g.get("m1_sweep"))     # "696"
    g.set("m1_sweep", "800")
    g.save()
    params = g.dump()            # dict of all parameters
    g.disconnect()
"""

from __future__ import annotations

import time
from typing import Dict, Optional

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False


class SerialError(RuntimeError):
    """Raised when the Arduino returns an error response."""


class NotConnectedError(RuntimeError):
    """Raised when an operation is attempted without an open connection."""


class GaugeSerial:
    """Serial communication interface for Gauge V4 firmware."""

    DEFAULT_BAUD = 115200
    DEFAULT_TIMEOUT = 2.0  # seconds per readline

    def __init__(self) -> None:
        self._ser: Optional["serial.Serial"] = None

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    @staticmethod
    def list_ports() -> "List[str]":
        """Return a list of available serial port device names."""
        if not HAS_SERIAL:
            return []
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port: str, baud: int = DEFAULT_BAUD) -> None:
        """Open *port* at *baud* and wait for the Arduino to reset."""
        if not HAS_SERIAL:
            raise RuntimeError("pyserial is not installed. Run: pip install pyserial")
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = serial.Serial(port, baud, timeout=self.DEFAULT_TIMEOUT)
        # Wait for Arduino bootloader to exit after DTR reset
        time.sleep(2.0)
        # Flush any startup text
        self._ser.reset_input_buffer()

    def disconnect(self) -> None:
        """Close the serial port."""
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    @property
    def is_connected(self) -> bool:
        return bool(self._ser and self._ser.is_open)

    # ------------------------------------------------------------------
    # Low-level I/O
    # ------------------------------------------------------------------

    def _send(self, line: str) -> str:
        """Send *line* (without newline) and return the stripped response."""
        if not self.is_connected:
            raise NotConnectedError("Not connected to Arduino")
        assert self._ser is not None
        self._ser.write((line + "\n").encode("ascii"))
        response = self._ser.readline().decode("ascii", errors="replace").strip()
        return response

    def _expect_ok(self, line: str) -> None:
        """Send *line* and raise SerialError if response is not 'ok'."""
        resp = self._send(line)
        if resp != "ok":
            raise SerialError(f"Command '{line}' failed: {resp}")

    # ------------------------------------------------------------------
    # Protocol commands
    # ------------------------------------------------------------------

    def ping(self) -> bool:
        """Return True if Arduino responds 'pong'."""
        try:
            return self._send("ping") == "pong"
        except NotConnectedError:
            return False

    def version(self) -> str:
        """Return firmware version string (e.g. 'version 4.0')."""
        return self._send("version")

    def get(self, param: str) -> str:
        """Read one parameter by name.  Returns the value as a string.

        Raises SerialError if Arduino responds with 'err: …'.
        """
        resp = self._send(f"get {param}")
        if resp.startswith("err:"):
            raise SerialError(f"get {param}: {resp}")
        # Response format: "<param> <value>"
        parts = resp.split(" ", 1)
        if len(parts) == 2:
            return parts[1]
        return resp

    def set(self, param: str, value: str) -> None:
        """Write one parameter value to Arduino RAM.

        Changes are not persisted until :meth:`save` is called.
        Raises SerialError on failure.
        """
        self._expect_ok(f"set {param} {value}")

    def save(self) -> None:
        """Flush all RAM parameters to EEPROM."""
        self._expect_ok("save")

    def load(self) -> None:
        """Re-read all parameters from EEPROM, discarding unsaved RAM changes."""
        self._expect_ok("load")

    def reset(self) -> None:
        """Reset all parameters to firmware defaults (RAM only until saved)."""
        self._expect_ok("reset")

    def dump(self) -> Dict[str, str]:
        """Read all parameters.  Returns a dict mapping name → value string."""
        if not self.is_connected:
            raise NotConnectedError("Not connected to Arduino")
        assert self._ser is not None
        self._ser.write(b"dump\n")
        params: Dict[str, str] = {}
        while True:
            line = self._ser.readline().decode("ascii", errors="replace").strip()
            if line == "end":
                break
            if not line:
                continue
            parts = line.split(" ", 1)
            if len(parts) == 2:
                params[parts[0]] = parts[1]
        return params

    # ------------------------------------------------------------------
    # Splash screen upload
    # ------------------------------------------------------------------

    def splash_begin(self, slot: int) -> None:
        """Start a splash upload for *slot* (1 or 2)."""
        resp = self._send(f"splash begin {slot}")
        if resp != "ready":
            raise SerialError(f"splash begin {slot}: {resp}")

    def splash_data(self, chunk: bytes) -> None:
        """Send up to 32 bytes as a hex-encoded data chunk."""
        if len(chunk) > 32:
            raise ValueError("chunk must be ≤ 32 bytes")
        self._expect_ok(f"splash data {chunk.hex()}")

    def splash_end(self, crc32_value: int) -> None:
        """Finalize upload with CRC-32 verification."""
        self._expect_ok(f"splash end {crc32_value:08x}")

    def splash_test(self, slot: int) -> None:
        """Ask Arduino to display the uploaded splash immediately."""
        self._expect_ok(f"splash test {slot}")

    def splash_clear(self, slot: int) -> None:
        """Erase the splash slot (revert to built-in default)."""
        self._expect_ok(f"splash clear {slot}")
