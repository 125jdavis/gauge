"""GaugeSerial get/set recovery when the gauge drops received bytes."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import serial_protocol as sp  # noqa: E402


class LossyGauge:
    """Stands in for the serial port: parses lines like the firmware and can drop bytes.

    drops maps a write number (0-based) to a slice of that write's bytes to remove,
    the way an interrupt-blocking LED update loses incoming UART bytes.
    """

    def __init__(self, drops=None):
        self.drops = dict(drops or {})
        self.writes = 0
        self.buf = b""
        self.replies = []
        self.params = {}
        self.is_open = True

    def write(self, data):
        drop = self.drops.get(self.writes)
        self.writes += 1
        if drop is not None:
            start, stop, _ = drop.indices(len(data))
            data = data[:start] + data[stop:]
        for byte in data:
            ch = bytes([byte])
            if ch == b"\n":
                if self.buf:
                    self.replies.append(self._handle(self.buf.decode()))
                self.buf = b""
            else:
                self.buf += ch

    def _handle(self, cmd):
        verb, _, rest = cmd.partition(" ")
        if verb == "set":
            param, space, value = rest.partition(" ")
            if not space or not value:
                return "err: type"
            self.params[param] = value
            return "ok"
        if verb == "get":
            return f"{rest} {self.params[rest]}" if rest in self.params else "err: unknown"
        return "err: unknown"

    def readline(self):
        return (self.replies.pop(0) + "\r\n").encode() if self.replies else b""

    def reset_input_buffer(self):
        self.replies.clear()


@pytest.fixture
def make():
    def build(drops=None):
        gauge = LossyGauge(drops)
        link = sp.GaugeSerial()
        link._ser = gauge
        link.RESYNC_WAIT = 0
        return link, gauge
    return build


def test_set_retries_when_the_value_is_lost(make):
    # 'set m1_func 0\n' loses 'c 0' -> 'set m1_fun' (no value) -> err: type, then the retry lands
    link, gauge = make({0: slice(10, 13)})
    link.set("m1_func", "0")
    assert gauge.params == {"m1_func": "0"}


def test_set_recovers_when_the_newline_is_lost_and_lines_merge(make):
    # The first newline is dropped: no reply, then the next command's reply would be off by one
    link, gauge = make({0: slice(-1, None)})
    link.set("m1_func", "0")
    link.set("m2_func", "1")
    link.set("m3_func", "0")
    assert gauge.params["m1_func"] == "0"
    assert gauge.params["m2_func"] == "1" and gauge.params["m3_func"] == "0"


def test_get_retries_until_the_reply_names_the_param(make):
    link, gauge = make({0: slice(4, 6)})
    gauge.params["ms_func"] = "1"
    assert link.get("ms_func") == "1"


def test_set_still_reports_a_real_rejection(make):
    link, gauge = make()
    gauge._handle = lambda cmd: "err: range"
    with pytest.raises(sp.SerialError, match="err: range"):
        link.set("ms_rpm_max", "50")
