"""
api.py
======
Python <-> JavaScript bridge for the web-view config tool (see app.py).

Every public method is callable from the page as ``await window.pywebview.api.<name>(...)``
and returns JSON-serialisable data. pywebview runs each call on its own thread, so serial
access is serialised with a lock. Long operations report back with ``self._emit(...)``,
which calls ``window.onGaugeEvent(evt)`` in the page.

Results are dicts: ``{"ok": True, ...}`` on success, or ``{"ok": False, "error": text,
"title": text}`` when something fails, so the page can show the same messages the Tk UI did.
Python stays the authority for validation, stored units and device I/O; the page only
converts values for display.

Names starting with an underscore are not exposed to JavaScript.
"""

from __future__ import annotations

import base64
import functools
import io
import json
import os
import threading
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

import webview

import gui_model as m
from config_file import ConfigFile

try:
    from serial_protocol import ConnectCancelled, GaugeSerial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    GaugeSerial = None  # type: ignore
    ConnectCancelled = RuntimeError  # type: ignore

try:
    from ssd1306_converter import SSD1306Converter
    from ssd1306_converter import HAS_PIL
except ImportError:
    HAS_PIL = False
    SSD1306Converter = None  # type: ignore

try:
    from splash_uploader import SplashUploader
    HAS_UPLOADER = HAS_SERIAL and HAS_PIL
except ImportError:
    HAS_UPLOADER = False
    SplashUploader = None  # type: ignore

try:
    import firmware_uploader
    from firmware_uploader import FirmwareError, FirmwareUploader
    from stk500v2 import HAS_SERIAL as HAS_FIRMWARE
except ImportError:
    HAS_FIRMWARE = False
    firmware_uploader = None  # type: ignore
    FirmwareUploader = None  # type: ignore
    FirmwareError = RuntimeError  # type: ignore

PREFS_PATH = Path.home() / ".gauge_config_tool.json"
DEFAULT_PREFS = {"theme": "dark", "units": "imperial"}
PREF_CHOICES = {"theme": ("dark", "light"), "units": m.UNIT_SYSTEMS}
CONFIG_FILE_TYPES = ("Config files (*.txt)", "All files (*.*)")
IMAGE_FILE_TYPES = ("Image files (*.png;*.bmp;*.jpg;*.jpeg;*.gif)", "All files (*.*)")
FIRMWARE_FILE_TYPES = ("Firmware (*.hex)", "All files (*.*)")


class UserError(Exception):
    """A failure to show the user as-is, with a dialog title."""

    def __init__(self, title: str, message: str) -> None:
        super().__init__(message)
        self.title = title


def _bridge(title: str) -> Callable:
    """Wrap a bridge method so it returns {"ok": ...} instead of raising."""
    def wrap(fn: Callable) -> Callable:
        @functools.wraps(fn)
        def call(self: "Api", *args: Any) -> Dict[str, Any]:
            try:
                result = fn(self, *args)
            except UserError as exc:
                return {"ok": False, "title": exc.title, "error": str(exc)}
            except m.ConfigInvalid as exc:
                return {"ok": False, "title": exc.title, "error": str(exc)}
            except Exception as exc:  # surfaced in the page, like the Tk messageboxes
                return {"ok": False, "title": title, "error": str(exc) or type(exc).__name__}
            return {"ok": True, **(result or {})}
        return call
    return wrap


class Api:
    def __init__(self) -> None:
        self._window: Optional[webview.Window] = None
        self._serial = GaugeSerial() if HAS_SERIAL else None
        self._lock = threading.RLock()  # one serial conversation at a time
        self._config_file = ConfigFile()
        self._splash: Dict[int, bytes] = {}
        self._test_orig_sources: Dict[str, str] = {}  # Test tab: source param -> original value
        self._tach_test_on = False
        self._connect_cancel: Optional[threading.Event] = None
        self._fw_busy = False

    # ------------------------------------------------------------------ plumbing
    def _attach(self, window: webview.Window) -> None:
        self._window = window

    def _emit(self, type_: str, **data: Any) -> None:
        """Push {type: 'progress'|'log'|'status'|'connect'|'disconnected', ...} to the page."""
        if self._window:
            self._window.evaluate_js(f"window.onGaugeEvent && window.onGaugeEvent({json.dumps({'type': type_, **data})})")

    def _connected(self) -> bool:
        return bool(self._serial and self._serial.is_connected)

    def _require_connection(self) -> None:
        if not HAS_SERIAL or self._serial is None:
            raise UserError("Error", "pyserial is not installed.\nRun: pip install pyserial")
        if not self._serial.is_connected:
            raise UserError("Not connected", "Connect to the Arduino first.")

    def _file_dialog(self, kind: str, **kwargs: Any) -> Optional[str]:
        assert self._window is not None
        dialog = {"open": webview.FileDialog.OPEN, "save": webview.FileDialog.SAVE}[kind]
        result = self._window.create_file_dialog(dialog, **kwargs)
        if not result:
            return None
        return result if isinstance(result, str) else result[0]

    def _on_closing(self) -> None:
        """Window closing: restore Test-tab sources, stop the tach test, drop the port."""
        try:
            self._stop_tach_test()
            self._restore_sources()
        finally:
            with self._lock:
                if self._serial:
                    self._serial.disconnect()

    # ------------------------------------------------------------------ prefs and static data
    def get_prefs(self) -> Dict[str, Any]:
        prefs = dict(DEFAULT_PREFS)
        try:
            stored = json.loads(PREFS_PATH.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            stored = {}
        if isinstance(stored, dict):
            for key, choices in PREF_CHOICES.items():
                if stored.get(key) in choices:
                    prefs[key] = stored[key]
        return prefs

    def set_pref(self, key: str, value: Any) -> Dict[str, Any]:
        prefs = self.get_prefs()
        if key in PREF_CHOICES and value in PREF_CHOICES[key]:
            prefs[key] = value
            try:
                PREFS_PATH.write_text(json.dumps(prefs), encoding="utf-8")
            except OSError:
                pass  # Preferences are a convenience; never fail the UI over them
        return prefs

    def get_static(self) -> Dict[str, Any]:
        """Option lists, tooltips and parameter metadata the page renders from (single source)."""
        return {
            "version": m.TOOL_VERSION,
            "params": m.ALL_PARAMS,
            "tooltips": m.TOOLTIPS,
            "sources": m.SOURCE_OPTIONS,
            "sourceDependent": m.SOURCE_DEPENDENT_PARAMS,
            "serialSourceValues": m.SERIAL_SOURCE_VALUES,
            "screens": m.SCREEN_NAMES,
            "motorFunctions": m.MOTOR_FUNCTION_OPTIONS,
            "motorUnits": m.MOTOR_FUNCTION_UNITS,
            "motorRanges": m.MOTOR_FUNCTION_RANGES,
            "motorStepsPerDegree": m.MOTOR_STEPS_PER_DEGREE,
            "motorMaxAngle": m.MOTOR_MAX_ANGLE,
            "motorSweepStepsRange": m.MOTOR_SWEEP_STEPS_RANGE,
            "outputModes": m.OUTPUT_MODE_OPTIONS,
            "outputOnOps": m.OUTPUT_ON_OPS,
            "outputParams": m.OUTPUT_PARAM_OPTIONS,
            "outputUnits": m.OUTPUT_PARAM_UNITS,
            "outputKinds": m.OUTPUT_PARAM_KINDS,
            "outputRanges": m.OUTPUT_PARAM_RANGES,
            "outputPwmPoints": m.OUTPUT_PWM_POINTS,
            "outputFreqRange": m.OUTPUT_FREQ_RANGE,
            "thermTypes": m.THERM_TYPE_OPTIONS, "thermPresets": m.THERM_PRESETS,
            "thermTypeCustom": m.THERM_TYPE_CUSTOM, "thermPoints": m.THERM_POINTS,
            "fuelTypes": m.FUEL_TYPE_OPTIONS, "fuelPresets": m.FUEL_PRESETS,
            "fuelTypeCustom": m.FUEL_TYPE_CUSTOM, "fuelDefaultMidPct": m.FUEL_DEFAULT_MID_PCT,
            "avTypes": m.AV_TYPE_OPTIONS, "avPresets": m.AV_PRESETS,
            "avTypeNone": m.AV_TYPE_NONE, "avTypeCustom": m.AV_TYPE_CUSTOM, "avTypeLambda": m.AV_TYPE_LAMBDA,
            "canProtocols": m.CAN_PROTOCOL_OPTIONS,
            "tachSweeps": m.TACH_SWEEP_OPTIONS,
            "tachColors": m.TACH_COLORS,
            "unitSystemOptions": m.UNIT_SYSTEM_OPTIONS,
            "filterParams": m.FILTER_PARAMS,
            "warnDebounceParams": m.WARN_DEBOUNCE_PARAMS,
            "warnDebounceMaxS": m.WARN_DEBOUNCE_MAX_S,
            "testSignals": m.TEST_SIGNALS,
            "features": {
                "serial": HAS_SERIAL, "pil": HAS_PIL,
                "uploader": HAS_UPLOADER, "firmware": HAS_FIRMWARE,
            },
        }

    # ------------------------------------------------------------------ connection
    def list_ports(self) -> List[List[str]]:
        if not HAS_SERIAL or self._serial is None:
            return []
        return [list(p) for p in self._serial.list_port_details()]

    def _do_connect(self, port: str, baud: int) -> str:
        """Mirror of GaugeConfigApp._connect_with_progress's worker. Returns the version ('' = no ping)."""
        assert self._serial is not None
        cancel = threading.Event()
        self._connect_cancel = cancel
        try:
            with self._lock:
                try:
                    self._serial.connect(
                        port, baud,
                        progress=lambda msg: (not cancel.is_set()) and self._emit("connect", text=msg),
                        cancel=cancel,
                    )
                    if self._serial.ping():
                        return self._serial.version()
                    self._serial.disconnect()
                    return ""
                except Exception:
                    self._serial.disconnect()
                    raise
        finally:
            self._connect_cancel = None

    @_bridge("Connection error")
    def connect(self, port: str, baud: Any) -> Dict[str, Any]:
        if not HAS_SERIAL or self._serial is None:
            raise UserError("Error", "pyserial is not installed.\nRun: pip install pyserial")
        try:
            baud = int(baud)
        except (TypeError, ValueError):
            raise UserError("Error", "Invalid baud rate.") from None
        if not port:
            raise UserError("No port", "Select the gauge's port first.")
        try:
            version = self._do_connect(port, baud)
        except ConnectCancelled:
            return {"connected": False, "cancelled": True}
        if not version:
            return {"connected": False, "noPing": True,
                    "warning": "Connected but Arduino did not respond to ping."}
        return {"connected": True, "version": version}

    def connect_cancel(self) -> None:
        if self._connect_cancel is not None:
            self._connect_cancel.set()

    @_bridge("Error")
    def disconnect(self) -> Dict[str, Any]:
        self._stop_tach_test()
        self._restore_sources()
        with self._lock:
            if self._serial:
                self._serial.disconnect()
        return {"connected": False}

    # ------------------------------------------------------------------ configuration
    @_bridge("Read error")
    def read_device(self) -> Dict[str, Any]:
        """Mirror of GaugeConfigApp._read_from_device. Values are stored (metric) firmware values."""
        self._require_connection()
        self._stop_tach_test()
        log = self._restore_sources()
        with self._lock:
            params = self._serial.dump()
        if params.get("filter_format") != "7":
            raise ValueError("Firmware update required: the connected device uses an older filter format.")
        m._decode_config_filters(params)
        warning = None
        if "tach_flash" not in params:
            params["tach_flash"] = ""
            warning = ("The connected firmware does not provide Flash RPM. "
                       "Update it before writing tach settings.")
        known = set(m.ALL_PARAMS)
        values = {k: v for k, v in params.items() if k in known}
        return {"values": values, "count": len(params), "warning": warning, "log": log}

    @_bridge("Write error")
    def write_device(self, values: Dict[str, str], dirty: List[str], imperial: bool = False) -> Dict[str, Any]:
        """Validate, then write the edited parameters in ALL_PARAMS order and save to EEPROM.

        *values* is the full merged configuration in stored units; *dirty* lists edited params.
        """
        self._require_connection()
        self._stop_tach_test()
        log = self._restore_sources()
        values = {k: str(v) for k, v in values.items()}
        dirty_set = set(dirty)
        updates, newly_dirty = m.validate_config(values, dirty_set, bool(imperial))
        values.update(updates)
        dirty_set |= newly_dirty
        to_write = m.params_to_write(values, dirty_set)
        if not to_write:
            return {"written": [], "updates": updates, "nothing": True, "log": log}
        with self._lock:
            if any(p in m.FILTER_PARAMS for p in to_write):
                if self._serial.get("filter_format") != "7":
                    raise ValueError("Firmware update required: the connected device uses an older filter format.")
                for p in to_write:
                    if p in m.FILTER_PARAMS:
                        m._filter_raw_to_percent(int(values[p]))
            for p in to_write:
                self._serial.set(p, values[p])
            self._serial.save()
        return {"written": to_write, "updates": updates, "log": log}

    @_bridge("Error")
    def reset_defaults(self) -> Dict[str, Any]:
        """Device > Reset to defaults from the Tk UI: firmware reset, then read back."""
        self._require_connection()
        self._restore_sources()
        with self._lock:
            self._serial.reset()
        result = self.read_device()
        if not result["ok"]:
            raise UserError(result["title"], result["error"])
        result.pop("ok")
        return result

    @_bridge("Load error")
    def load_file(self) -> Dict[str, Any]:
        path = self._file_dialog("open", file_types=CONFIG_FILE_TYPES)
        if not path:
            return {"cancelled": True}
        params = self._config_file.load_from_file(path)
        values, system = m.migrate_loaded_config(params)
        return {"values": values, "system": system, "name": os.path.basename(path), "count": len(values)}

    @_bridge("Save error")
    def save_file(self, values: Dict[str, str], dirty: List[str], system: str) -> Dict[str, Any]:
        values = {k: str(v) for k, v in values.items()}
        updates, newly_dirty = m.validate_config(values, set(dirty), system == "imperial")
        values.update(updates)
        params = m.config_for_save(values, system)
        path = self._file_dialog("save", save_filename=ConfigFile.default_filename(),
                                 file_types=CONFIG_FILE_TYPES)
        if not path:
            return {"cancelled": True}
        if not os.path.splitext(path)[1]:
            path += ".txt"
        self._config_file.save_to_file(path, params)
        return {"path": path, "name": os.path.basename(path), "updates": updates,
                "newlyDirty": sorted(newly_dirty)}

    # ------------------------------------------------------------------ firmware
    def _describe_firmware(self, path: str) -> tuple:
        """Mirror of GaugeConfigApp._fw_describe: (usable, log lines)."""
        try:
            warnings = firmware_uploader.validate(path)
        except FirmwareError as exc:
            return False, [f"Cannot use this file: {exc}"]
        size = len(firmware_uploader.load_image(path))
        return True, [f"Ready to flash: {size / 1024:.1f} KB."] + [f"Note: {w}" for w in warnings]

    @_bridge("Error")
    def firmware_autoselect(self) -> Dict[str, Any]:
        """Pre-select a firmware image shipped alongside the tool (GaugeConfigApp._fw_autoselect)."""
        if not HAS_FIRMWARE:
            return {"path": "", "available": False,
                    "log": ["Firmware update unavailable: pyserial is not installed."]}
        candidates = firmware_uploader.list_firmware()
        if not candidates:
            return {"path": "", "available": True,
                    "log": ["Select the firmware file supplied with your gauge, then press Update Firmware."]}
        path = candidates[0]
        log = [f"Found firmware: {os.path.basename(path)}"]
        if len(candidates) > 1:
            log.append(f"({len(candidates) - 1} other .hex file(s) nearby — use Browse if this is not the right one.)")
        log += self._describe_firmware(path)[1]
        return {"path": path, "available": True, "log": log}

    @_bridge("Error")
    def firmware_browse(self, current: str = "") -> Dict[str, Any]:
        current = (current or "").strip()
        if current and os.path.isfile(current):
            directory = os.path.dirname(current)
        else:
            dirs = firmware_uploader.firmware_search_dirs() if HAS_FIRMWARE else []
            directory = dirs[0] if dirs else os.getcwd()
        path = self._file_dialog("open", directory=directory, file_types=FIRMWARE_FILE_TYPES)
        if not path:
            return {"cancelled": True}
        if not HAS_FIRMWARE:
            return {"path": path, "usable": False, "log": []}
        usable, lines = self._describe_firmware(path)
        return {"path": path, "usable": usable, "log": ["", f"Selected {os.path.basename(path)}", *lines]}

    @_bridge("Cannot use this firmware file")
    def firmware_check(self, path: str) -> Dict[str, Any]:
        if not HAS_FIRMWARE:
            raise UserError("Unavailable", "pyserial is required. Run: pip install pyserial")
        if self._fw_busy:
            raise UserError("Busy", "A firmware update is already running.")
        return {"warnings": firmware_uploader.validate((path or "").strip())}

    @_bridge("Firmware update failed")
    def firmware_update(self, path: str, port: str, baud: Any) -> Dict[str, Any]:
        """Drop the connection, flash on this thread, then reconnect if it was connected.

        Mirrors GaugeConfigApp._fw_upload / _fw_worker / _fw_finish / _fw_reconnect.
        Progress and log lines arrive as 'progress' and 'log' events with target 'firmware'.
        """
        if not HAS_FIRMWARE:
            raise UserError("Unavailable", "pyserial is required. Run: pip install pyserial")
        if self._fw_busy:
            raise UserError("Busy", "A firmware update is already running.")
        path = (path or "").strip()
        firmware_uploader.validate(path)
        port = (port or "").strip()
        if not port:
            raise UserError("No port", "Select the gauge's port first, then press Update Firmware.")

        def log(line: str) -> None:
            self._emit("log", target="firmware", text=line)

        self._fw_busy = True
        try:
            was_connected = self._connected()
            if was_connected:
                self._stop_tach_test()
                self._restore_sources()
                with self._lock:
                    self._serial.disconnect()
                self._emit("disconnected", reason="updating")
            self._emit("progress", target="firmware", pct=0)
            log("")
            log(f"--- Updating firmware on {port} ---")
            try:
                FirmwareUploader().upload(
                    path, port, log_cb=log,
                    progress_cb=lambda pct: self._emit("progress", target="firmware", pct=pct),
                )
            except FirmwareError as exc:
                error = str(exc)
            except Exception as exc:
                error = f"Unexpected error: {exc}"
            else:
                error = ""
            if error:
                self._emit("progress", target="firmware", pct=0)
                log(f"UPDATE FAILED: {error}")
            else:
                self._emit("progress", target="firmware", pct=100)

            result: Dict[str, Any] = {"updated": not error, "error": error, "reconnected": False}
            if not was_connected:
                if not error:
                    log("Press Connect to verify the new firmware.")
                return result
            log(f"Waiting {m.BOOTLOADER_EXIT_MS / 1000:.0f}s for the gauge to restart...")
            time.sleep(m.BOOTLOADER_EXIT_MS / 1000)
            try:
                version = self._do_connect(port, int(baud))
            except ConnectCancelled:
                log("Reconnect cancelled.")
                return result
            except Exception as exc:
                log(f"Reconnect failed: {exc}")
                return result
            if not version:
                log("The gauge did not respond after the update. "
                    "Unplug it, plug it back in, then press Connect.")
                return result
            log(f"Reconnected — {version}")
            result.update(reconnected=True, version=version)
            return result
        finally:
            self._fw_busy = False

    # ------------------------------------------------------------------ splash screens
    @_bridge("Load error")
    def splash_load(self, slot: int) -> Dict[str, Any]:
        path = self._file_dialog("open", file_types=IMAGE_FILE_TYPES)
        if not path:
            return {"cancelled": True}
        if not HAS_PIL:
            raise UserError("Error", "Pillow is required: pip install Pillow")
        conv = SSD1306Converter()
        data = conv.convert(path)
        self._splash[int(slot)] = data
        buf = io.BytesIO()
        conv.preview(data).save(buf, format="PNG")
        png = "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode("ascii")
        return {"name": os.path.basename(path), "png": png}

    @_bridge("Upload error")
    def splash_upload(self, slot: int) -> Dict[str, Any]:
        slot = int(slot)
        if slot not in self._splash:
            raise UserError("No image", f"Load an image for Slot {slot} first.")
        self._require_connection()
        if not HAS_UPLOADER:
            raise UserError("Error", "pyserial and Pillow are required for upload.")
        target = f"splash{slot}"
        self._emit("progress", target=target, pct=0)
        with self._lock:
            SplashUploader(self._serial).upload_bytes(
                slot, self._splash[slot],
                progress_cb=lambda pct: self._emit("progress", target=target, pct=pct),
            )
        self._emit("progress", target=target, pct=100)
        return {}

    @_bridge("Error")
    def splash_test(self, slot: int) -> Dict[str, Any]:
        self._require_connection()
        with self._lock:
            self._serial.splash_test(int(slot))
        return {}

    @_bridge("Error")
    def splash_clear(self, slot: int) -> Dict[str, Any]:
        self._require_connection()
        with self._lock:
            self._serial.splash_clear(int(slot))
        self._splash.pop(int(slot), None)
        return {}

    # ------------------------------------------------------------------ LED tach live test
    @_bridge("Tach test error")
    def tach_preview(self, values: Dict[str, str]) -> Dict[str, Any]:
        """Start or refresh the strip test with unsaved settings (the page repeats this every second)."""
        self._require_connection()
        payload = m.tach_preview_payload({k: str(v) for k, v in values.items()})
        with self._lock:
            self._serial.tach_preview(payload)
        self._tach_test_on = True
        return {}

    def _stop_tach_test(self, raise_errors: bool = False) -> None:
        """Release the strip test. Failures are ignored unless asked: the preview expires by itself."""
        if not self._tach_test_on:
            return
        self._tach_test_on = False
        if self._connected():
            try:
                with self._lock:
                    self._serial.tach_preview_stop()
            except Exception:
                if raise_errors:
                    raise

    @_bridge("Tach test error")
    def tach_preview_stop(self) -> Dict[str, Any]:
        try:
            self._stop_tach_test(raise_errors=True)
        except Exception as exc:
            raise UserError("Tach test error", f"{exc}\nThe preview expires automatically.") from None
        return {}

    # ------------------------------------------------------------------ Test tab
    def _test_run(self, desc: str, action: Callable[[], Any], log: List[str]) -> bool:
        """Run a serial action and log it as the Tk UI's _test_run did."""
        try:
            with self._lock:
                result = action()
        except Exception as exc:
            log.append(f"{desc} -> ERROR: {exc}")
            return False
        log.append(f"{desc} -> {result if result else 'ok'}")
        return True

    def _use_serial_source(self, param: str, log: List[str]) -> bool:
        """Temporarily switch *param* to its Serial source in RAM, remembering the original."""
        if param in self._test_orig_sources:
            return True
        try:
            with self._lock:
                original = self._serial.get(param)
        except Exception as exc:
            log.append(f"get {param} -> ERROR: {exc}")
            return False
        value = str(m.SERIAL_SOURCE_VALUES[param])
        if not self._test_run(f"set {param} {value}", lambda: self._serial.set(param, value), log):
            return False
        self._test_orig_sources[param] = original
        return True

    def _restore_sources(self) -> List[str]:
        """Put every source changed by the Test tab back (RAM only, never saved)."""
        pending, self._test_orig_sources = self._test_orig_sources, {}
        log: List[str] = []
        if pending and self._connected():
            for param, original in pending.items():
                self._test_run(f"restore {param} {original}",
                               lambda p=param, o=original: self._serial.set(p, o), log)
        return log

    @_bridge("Test failed")
    def test_signal(self, key: str, value: float) -> Dict[str, Any]:
        """key: speed|rpm|oil|coolant|fuel. *value* is already in metric firmware units."""
        self._require_connection()
        spec = next(s for s in m.TEST_SIGNALS if s[0] == key)
        method, source = spec[7], spec[8]
        metric: Any = float(value)
        if key in ("speed", "rpm"):
            metric = int(round(metric))
        log: List[str] = []
        ok = self._use_serial_source(source, log) and self._test_run(
            f"{key} = {metric:g}", lambda: getattr(self._serial, method)(metric), log)
        return {"sent": ok, "log": log}

    @_bridge("Error")
    def restore_sources(self) -> Dict[str, Any]:
        return {"log": self._restore_sources()}

    @_bridge("Test failed")
    def test_odo(self, revs: Any) -> Dict[str, Any]:
        self._require_connection()
        try:
            revs = int(revs)
        except (TypeError, ValueError):
            raise UserError("Invalid value", "Revolutions must be an integer.") from None
        if not -100 <= revs <= 100:
            raise UserError("Out of range", "Revolutions must be between -100 and 100.")
        log: List[str] = []
        ok = revs != 0 and self._test_run(f"odo motor {revs}", lambda: self._serial.test_odo_motor(revs), log)
        # Motor runs at ~4 RPM (15 s per revolution); the page reverts its button after this long
        return {"moving": ok, "ms": abs(revs) * 15000 + 1000, "log": log}

    @_bridge("Test failed")
    def test_odo_stop(self) -> Dict[str, Any]:
        self._require_connection()
        log: List[str] = []
        self._test_run("odo stop", lambda: self._serial.test_odo_stop(), log)
        return {"log": log}

    @_bridge("Test failed")
    def ping(self) -> Dict[str, Any]:
        self._require_connection()
        log: List[str] = []
        self._test_run("ping", lambda: "pong" if self._serial.ping() else "no response", log)
        return {"log": log}

    @_bridge("Test failed")
    def version(self) -> Dict[str, Any]:
        self._require_connection()
        log: List[str] = []
        self._test_run("version", lambda: self._serial.version(), log)
        return {"log": log}

    @_bridge("Test failed")
    def test_splash(self, slot: int) -> Dict[str, Any]:
        self._require_connection()
        log: List[str] = []
        self._test_run(f"splash test {slot}", lambda: self._serial.splash_test(int(slot)), log)
        return {"log": log}
