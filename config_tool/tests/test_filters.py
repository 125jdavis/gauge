import os
import sys
import tkinter as tk
from pathlib import Path

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from gui import (
    FILTER_PARAMS,
    GaugeConfigApp,
    _decode_config_filters,
    _filter_percent_to_raw,
    _filter_raw_to_percent,
    _source_uses_parameter,
)
import gui


def test_percent_conversion_all_slider_values() -> None:
    values = [_filter_percent_to_raw(percent) for percent in range(100)]
    assert values[0] == 0
    assert values[50] == 64
    assert values[99] == 127
    assert values == sorted(set(values))
    assert [_filter_raw_to_percent(raw) for raw in values] == list(range(100))
    for raw in range(128):
        assert 0 <= _filter_raw_to_percent(raw) <= 99
    for invalid in (-1, 100):
        with pytest.raises(ValueError):
            _filter_percent_to_raw(invalid)
    for invalid in (-1, 128, 256):
        with pytest.raises(ValueError):
            _filter_raw_to_percent(invalid)


def test_legacy_filter_conversion_matches_old_sample_weights() -> None:
    params = {
        "filter_vbatt": "8", "filter_fuel": "1", "filter_therm": "50",
        "filter_av1": "4", "filter_av2": "12", "filter_av3": "12",
        "filter_hall": "64", "filter_rpm": "179",
    }
    assert _decode_config_filters(params) == {
        "filter_vbatt": "112", "filter_fuel": "126", "filter_therm": "64",
        "filter_av1": "96", "filter_av2": "32", "filter_av3": "32",
        "filter_hall": "96", "filter_rpm": "39",
    }
    assert _decode_config_filters({"av1_type": "3", "filter_av1": "4"}) == {
        "filter_av1": "120",
    }
    assert _decode_config_filters({"av1_type": "2", "filter_av1": "255"}) == {
        "filter_av1": "0",
    }


def test_new_filter_format_round_trips_and_rejects_bad_values() -> None:
    for raw in range(128):
        data = {"filter_format": "7", **{key: str(raw) for key in FILTER_PARAMS}}
        assert _decode_config_filters(data) == {key: str(raw) for key in FILTER_PARAMS}
    for data in (
        {"filter_format": "8"}, {"filter_format": "7", "filter_rpm": "128"},
        {"filter_vbatt": "256"}, {"filter_therm": "0"}, {"filter_gps": "0"},
        {"av1_type": "3", "filter_av1": "65"}, {"filter_av1": "256"},
    ):
        with pytest.raises(ValueError):
            _decode_config_filters(data)


def test_gps_filter_only_enabled_for_gps_source() -> None:
    assert _source_uses_parameter("filter_gps", {"speed_source": "3"})
    assert not _source_uses_parameter("filter_gps", {"speed_source": "2"})


def test_slider_reads_without_changing_raw_and_writes_percent() -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    try:
        app._vars["speed_source"].set("2")
        for param in FILTER_PARAMS:
            app._vars[param].set("1")
        app.root.update()
        assert all(app._vars[param].get() == "1" for param in FILTER_PARAMS)

        slider = app._param_widgets["filter_hall"]
        assert not slider.instate(["disabled"])
        assert app._param_widgets["filter_gps"].instate(["disabled"])
        slider.set(99)
        app.root.update()
        assert app._vars["filter_hall"].get() == "127"
        slider.set(0)
        app.root.update()
        assert app._vars["filter_hall"].get() == "0"
        app._vars["speed_source"].set("3")
        assert app._param_widgets["filter_hall"].instate(["disabled"])
        assert not app._param_widgets["filter_gps"].instate(["disabled"])
    finally:
        app.root.destroy()


def test_gui_filter_protocol_and_file_migration(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")

    class FakeSerial:
        is_connected = True
        marker = "7"

        def __init__(self) -> None:
            self.writes: list[tuple[str, str]] = []

        def dump(self) -> dict[str, str]:
            return {"filter_format": self.marker, "filter_vbatt": "1"}

        def get(self, param: str) -> str:
            assert param == "filter_format"
            return self.marker

        def set(self, param: str, value: str) -> None:
            self.writes.append((param, value))

        def save(self) -> None:
            pass

    serial = FakeSerial()
    errors: list[str] = []
    monkeypatch.setattr(gui.messagebox, "showerror", lambda _title, text: errors.append(text))
    for method in (
        "_validate_tach", "_validate_motor_angles", "_validate_speed_inputs",
        "_validate_warning_inputs", "_validate_fuel_capacity", "_validate_motor_ranges",
    ):
        monkeypatch.setattr(app, method, lambda: True)
    app._serial = serial
    try:
        app._read_from_device()
        app.root.update()
        assert app._vars["filter_vbatt"].get() == "1"
        assert not app._dirty["filter_vbatt"]
        app._param_widgets["filter_vbatt"].set(99)
        app.root.update()
        app._write_to_device()
        assert serial.writes == [("filter_vbatt", "127")]
        assert not app._dirty["filter_vbatt"]

        serial.marker = "old"
        app._vars["filter_vbatt"].set("64")
        app._write_to_device()
        assert len(serial.writes) == 1
        assert "Firmware update required" in errors[-1]
        app._read_from_device()
        assert app._vars["filter_vbatt"].get() == "64"
        assert "Firmware update required" in errors[-1]

        path = tmp_path / "legacy.txt"
        path.write_text("config_format=5\nfilter_vbatt=8\nfilter_rpm=179\n", encoding="utf-8")
        monkeypatch.setattr(gui.filedialog, "askopenfilename", lambda **_kwargs: str(path))
        app._load_from_file()
        assert app._vars["filter_vbatt"].get() == "112"
        assert app._vars["filter_rpm"].get() == "39"
        assert app._dirty["filter_vbatt"]

        monkeypatch.setattr(gui.filedialog, "asksaveasfilename", lambda **_kwargs: str(path))
        app._save_to_file()
        saved = path.read_text(encoding="utf-8")
        assert "filter_format=7\n" in saved
        assert "filter_vbatt=112\n" in saved
    finally:
        app.root.destroy()
