"""Starter Python <-> JS bridge for the web-view config tool.

Every public method is callable from JS as `await window.pywebview.api.<name>(...)`
and must return JSON-serialisable data. Long operations run on a worker thread
and report back through `self.emit(...)`, which calls `window.onGaugeEvent(evt)`
in the page.

Bodies marked TODO should be lifted from the matching handler in gui.py —
that code is the source of truth for ordering, retries and error handling.
"""
from __future__ import annotations

import base64
import io
import json
import os
import threading
from pathlib import Path
from typing import Any, Dict, List, Optional

import webview

import gui_model as m  # module-level constants + helpers moved out of gui.py
from config_file import ConfigFile

try:
    from serial_protocol import GaugeSerial, SerialError, NotConnectedError
except ImportError:
    GaugeSerial = None  # type: ignore

PREFS = Path.home() / ".gauge_config_tool.json"


class Api:
    def __init__(self) -> None:
        self._window: Optional[webview.Window] = None
        self._serial = GaugeSerial() if GaugeSerial else None
        self._config_file = ConfigFile()
        self._splash: Dict[int, bytes] = {}
        self._overridden_sources: Dict[str, str] = {}  # Test tab: source param -> original value

    # ---- plumbing ----
    def attach(self, window: webview.Window) -> None:
        self._window = window

    def emit(self, type_: str, **data: Any) -> None:
        """Push an event to the page: {type: 'progress'|'log'|'status'|'disconnected', ...}."""
        if self._window:
            self._window.evaluate_js(f"window.onGaugeEvent({json.dumps({'type': type_, **data})})")

    def _bg(self, fn, *args) -> None:
        threading.Thread(target=fn, args=args, daemon=True).start()

    def on_closing(self) -> None:
        self.restore_sources()
        self.tach_preview_stop()
        self.disconnect()

    # ---- app state & prefs ----
    def get_prefs(self) -> Dict[str, Any]:
        try:
            return {"theme": "dark", "units": "imperial", **json.loads(PREFS.read_text())}
        except (OSError, ValueError):
            return {"theme": "dark", "units": "imperial"}

    def set_pref(self, key: str, value: Any) -> None:
        prefs = self.get_prefs()
        prefs[key] = value
        PREFS.write_text(json.dumps(prefs))

    def get_static(self) -> Dict[str, Any]:
        """Option lists, tooltips and parameter metadata the UI renders from (single source: gui_model)."""
        return {
            "version": m.TOOL_VERSION,
            "params": m.ALL_PARAMS,
            "tooltips": m.TOOLTIPS,
            "sources": m.SOURCE_OPTIONS,
            "sourceDependent": m.SOURCE_DEPENDENT_PARAMS,
            "screens": m.SCREEN_NAMES,
            "motorFunctions": m.MOTOR_FUNCTION_OPTIONS,
            "motorRanges": m.MOTOR_FUNCTION_RANGES,
            "outputParams": m.OUTPUT_PARAM_OPTIONS,
            "outputKinds": m.OUTPUT_PARAM_KINDS,
            "thermTypes": m.THERM_TYPE_OPTIONS, "thermPresets": m.THERM_PRESETS,
            "fuelTypes": m.FUEL_TYPE_OPTIONS, "fuelPresets": m.FUEL_PRESETS,
            "avTypes": m.AV_TYPE_OPTIONS, "avPresets": m.AV_PRESETS,
            "canProtocols": m.CAN_PROTOCOL_OPTIONS, "tachSweeps": m.TACH_SWEEP_OPTIONS,
        }

    # ---- connection ----
    def list_ports(self) -> List[List[str]]:
        return [list(p) for p in GaugeSerial.list_port_details()] if GaugeSerial else []

    def connect(self, port: str, baud: int) -> Dict[str, Any]:
        # TODO: mirror gui.py's connect flow (wait/cancel handling, ConnectCancelled).
        self._serial.connect(port, int(baud))
        return {"connected": True, "version": self._serial.version()}

    def disconnect(self) -> None:
        if self._serial and self._serial.is_connected():
            self._serial.disconnect()

    # ---- configuration (raw firmware values, always metric) ----
    def read_device(self) -> Dict[str, str]:
        return self._serial.dump()

    def write_device(self, params: Dict[str, str]) -> Dict[str, Any]:
        """Validate, then write params in gui.py's order (output mode last), then save to EEPROM."""
        try:
            m.validate_outputs(params)
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        # TODO: copy gui.py's write loop (skips inactive output params via output_param_active,
        # writes curve voltages, ordering) and call self._serial.save() at the end.
        return {"ok": True}

    def load_file(self) -> Optional[Dict[str, str]]:
        paths = self._window.create_file_dialog(webview.OPEN_DIALOG, file_types=("Config files (*.txt)", "All files (*.*)"))
        if not paths:
            return None
        params = self._config_file.load_from_file(paths[0])
        # TODO: run gui.py's migration: m._decode_config_units(params), m._decode_config_filters(params).
        return params

    def save_file(self, params: Dict[str, str]) -> Optional[str]:
        path = self._window.create_file_dialog(webview.SAVE_DIALOG, save_filename=ConfigFile.default_filename())
        if not path:
            return None
        path = path if isinstance(path, str) else path[0]
        self._config_file.save_to_file(path, params)
        return os.path.basename(path)

    # ---- firmware ----
    def firmware_files(self) -> List[str]:
        import firmware_uploader
        return firmware_uploader.list_firmware()

    def firmware_update(self, path: str) -> None:
        # TODO: run FirmwareUploader.upload on a worker thread exactly as gui.py does,
        # forwarding its progress/log callbacks: self.emit("progress", target="firmware", pct=..)
        # and self.emit("log", target="firmware", text=..).
        pass

    # ---- splash screens ----
    def splash_load(self, slot: int) -> Optional[Dict[str, str]]:
        from ssd1306_converter import SSD1306Converter
        paths = self._window.create_file_dialog(webview.OPEN_DIALOG, file_types=("Images (*.png;*.bmp;*.jpg;*.gif)",))
        if not paths:
            return None
        conv = SSD1306Converter()
        data = conv.convert(paths[0])
        self._splash[slot] = data
        buf = io.BytesIO()
        conv.preview(data).save(buf, format="PNG")
        return {"name": os.path.basename(paths[0]), "png": "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()}

    def splash_upload(self, slot: int) -> None:
        # TODO: SplashUploader(...).upload_bytes(...) on a worker thread, emitting progress.
        pass

    def splash_test(self, slot: int) -> None:
        self._serial.splash_test(slot)

    def splash_clear(self, slot: int) -> None:
        self._serial.splash_clear(slot)
        self._splash.pop(slot, None)

    # ---- LED tach live test ----
    def tach_preview(self, payload: str) -> None:
        self._serial.tach_preview(payload)

    def tach_preview_stop(self) -> None:
        if self._serial and self._serial.is_connected():
            self._serial.tach_preview_stop()

    # ---- Test tab ----
    def test_signal(self, name: str, value: float) -> None:
        """name: speed|rpm|oil|coolant|fuel. Value in metric firmware units."""
        # TODO: as gui.py — remember the original source, set it to m.SERIAL_SOURCE_VALUES in RAM only.
        getattr(self._serial, {"speed": "test_speed", "rpm": "test_rpm", "oil": "test_oil_pressure",
                               "coolant": "test_coolant_temp", "fuel": "test_fuel_level"}[name])(value)

    def restore_sources(self) -> None:
        # TODO: as gui.py — set each overridden source back without saving to EEPROM.
        self._overridden_sources.clear()

    def test_odo(self, revs: int) -> None:
        self._serial.test_odo_motor(int(revs))

    def ping(self) -> bool:
        return self._serial.ping()

    def version(self) -> str:
        return self._serial.version()
