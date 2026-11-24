"""Data logging: stream line parsing, CSV recording, serial handling and the bridge."""
import io
import os
import sys
import threading
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import datalog  # noqa: E402
import serial_protocol as sp  # noqa: E402

LINE = "L,123456,6000,3200,3500,4000,987,885,1013,1384"


def test_parse_line_scales_to_metric_units():
    ms, values = datalog.parse_line(LINE)
    assert ms == 123456
    assert values == [60.0, 3200, 350.0, 400.0, 0.987, 88.5, 101.3, 13.84]


@pytest.mark.parametrize("line", ["", "ok", "L,1,2,3", "L,1,2,3,4,5,6,7,8,x", LINE + ",5"])
def test_parse_line_rejects_partial_or_foreign_lines(line):
    assert datalog.parse_line(line) is None


def test_csv_recorder_writes_imperial_units_and_relative_time():
    buf = io.StringIO()
    rec = datalog.CsvRecorder(buf, imperial=True)
    _, values = datalog.parse_line(LINE)
    rec.write(10000, values)
    rec.write(10100, values)
    rows = buf.getvalue().splitlines()
    assert rows[0] == ("Time (s),Vehicle speed (mph),Engine RPM (rpm),Oil pressure (psi),"
                       "Fuel pressure (psi),Lambda (λ),Coolant temp (°F),MAP (psi (abs)),Battery voltage (V)")
    assert rows[1] == "0.000,37,3200,50.8,58.0,0.987,191.3,14.7,13.84"
    assert rows[2].startswith("0.100,")
    assert rec.rows == 2


def test_csv_recorder_keeps_time_increasing_when_the_gauge_restarts():
    buf = io.StringIO()
    rec = datalog.CsvRecorder(buf, imperial=False)
    _, values = datalog.parse_line(LINE)
    for ms in (50000, 50100, 3000, 3100):
        rec.write(ms, values)
    times = [row.split(",")[0] for row in buf.getvalue().splitlines()[1:]]
    assert times == ["0.000", "0.100", "0.200", "0.300"]


class StreamingPort:
    """Serial port stand-in whose replies are preceded by data-logging lines."""

    def __init__(self, replies):
        self.lines = list(replies)
        self.written = []
        self.is_open = True
        self.timeout = 2.0

    def write(self, data):
        self.written.append(data.decode())

    def readline(self):
        return (self.lines.pop(0) + "\r\n").encode() if self.lines else b""

    def reset_input_buffer(self):
        self.lines.clear()


def _gauge(replies):
    g = sp.GaugeSerial()
    g._ser = StreamingPort(replies)
    return g


def test_replies_skip_data_lines_that_arrive_first():
    g = _gauge([LINE, LINE, "m1_sweep 696"])
    assert g.get("m1_sweep") == "696"


def test_log_start_and_stop():
    g = _gauge(["ok"])
    g.log_start()
    assert g._ser.timeout == sp.GaugeSerial.LOG_READ_TIMEOUT
    g._ser.lines = [LINE, "ok"]
    g.log_stop()
    assert g._ser.written == ["log start\n", "log stop\n"]
    assert g._ser.timeout == sp.GaugeSerial.DEFAULT_TIMEOUT


def test_log_start_reports_old_firmware(monkeypatch):
    monkeypatch.setattr(sp.GaugeSerial, "RESYNC_WAIT", 0)
    g = _gauge(["err: unknown"] * 3)
    with pytest.raises(sp.SerialError, match="does not support data logging"):
        g.log_start()


# ---- bridge ---------------------------------------------------------------------------

class LoggingSerial:
    """GaugeSerial stand-in for the bridge: streams queued lines while logging."""

    is_connected = True

    def __init__(self, lines):
        self.lines = list(lines)
        self.logging = False
        self.calls = []

    def log_start(self):
        self.calls.append("log start")
        self.logging = True

    def log_stop(self):
        self.calls.append("log stop")
        self.logging = False

    def read_log_line(self):
        if self.logging and self.lines:
            return self.lines.pop(0)
        time.sleep(0.01)
        return ""

    def disconnect(self):
        self.is_connected = False


@pytest.fixture
def bridge():
    pytest.importorskip("webview")
    import api
    instance = api.Api()
    instance._serial = LoggingSerial([LINE.replace("123456", str(1000 + 100 * i)) for i in range(5)])
    instance.events = []
    instance._emit = lambda type_, **data: instance.events.append((type_, data))
    yield instance
    instance._stop_logging(quiet=True)


def _wait_for(predicate, timeout=2.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.01)
    raise AssertionError("timed out")


def test_log_start_streams_samples_and_blocks_other_commands(bridge):
    assert bridge.log_start()["ok"]
    _wait_for(lambda: len(bridge.events) == 5)
    assert bridge.events[0] == ("logSample", {"t": 1000, "v": datalog.parse_line(LINE)[1], "rows": 0})
    blocked = bridge.read_device()
    assert not blocked["ok"] and blocked["title"] == "Logging"
    assert bridge.log_stop()["ok"]
    assert bridge._serial.calls == ["log start", "log stop"]
    assert not any(t.name != "MainThread" and t.is_alive() and t is bridge._log_thread
                   for t in threading.enumerate())


def test_record_writes_a_csv_until_stopped(bridge, tmp_path):
    path = str(tmp_path / "run")
    bridge._file_dialog = lambda *a, **k: path
    res = bridge.log_record_start(False)
    assert res["ok"] and res["path"] == path + ".csv"
    _wait_for(lambda: len(bridge.events) == 5)
    res = bridge.log_record_stop()
    assert res["rows"] == 5
    assert bridge._log_stop_evt is not None  # live view keeps running
    with open(path + ".csv", encoding="utf-8-sig") as f:
        lines = f.read().splitlines()
    assert lines[0].startswith("Time (s),Vehicle speed (km/h)")
    assert [line.split(",")[0] for line in lines[1:]] == ["0.000", "0.100", "0.200", "0.300", "0.400"]


def test_lost_port_ends_logging_and_reports_it(bridge):
    def broken():
        raise OSError("device unplugged")
    bridge._serial.read_log_line = broken
    bridge.log_start()
    _wait_for(lambda: bridge.events)
    assert bridge.events[-1][0] == "logError"
    assert "device unplugged" in bridge.events[-1][1]["text"]
    assert bridge._log_stop_evt is None
