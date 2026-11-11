"""
gui.py
======
Tkinter-based GUI for the Gauge V4 Configuration Tool.

Implements CONFIG_TOOL_SPECIFICATION.md §12 (Python Application Specification):
  Tab 1  - Connection (and firmware update, §13)
  Tab 2  - Motor Configuration
  Tab 3  - Signal Source
  Tab 4  - Speed/RPM (including mechanical odometer calibration)
  Tab 5  - Sensors
  Tab 6  - LED Tachometer
  Tab 7  - Display Settings (including clock and fuel)
  Tab 8  - Fault Warnings
  Tab 9  - Splash Screens

Run with: python gui.py
"""

from __future__ import annotations

import math
import os
import queue
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog, scrolledtext
from typing import Callable, Dict, Optional

try:
    from serial_protocol import GaugeSerial, SerialError, NotConnectedError, ConnectCancelled
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    GaugeSerial = None  # type: ignore

try:
    from ssd1306_converter import SSD1306Converter
    HAS_PIL = True
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
    from firmware_uploader import FirmwareUploader, FirmwareError
    from stk500v2 import HAS_SERIAL as HAS_PYSERIAL
    HAS_FIRMWARE = HAS_PYSERIAL
except ImportError:
    HAS_FIRMWARE = False
    firmware_uploader = None  # type: ignore
    FirmwareUploader = None  # type: ignore
    FirmwareError = RuntimeError  # type: ignore

from config_file import ConfigFile

TOOL_VERSION = "1.0"
WINDOW_TITLE = f"Gauge V4 Configuration Tool v{TOOL_VERSION}"

SOURCE_OPTIONS: Dict[str, tuple] = {
    "speed_source": ("Off", "CAN", "Hall sensor", "GPS", "Synthetic", "Odometer test", "Serial"),
    "rpm_source": ("Off", "CAN", "Coil", "Synthetic", "Serial"),
    "oil_prs_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic", "Serial"),
    "fuel_prs_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic"),
    "coolant_src": ("Off", "CAN", "Thermistor", "Synthetic", "Serial"),
    "oil_temp_src": ("Off", "CAN", "Thermistor"),
    "map_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic"),
    "lambda_source": ("Off", "CAN", "AV1", "AV2", "AV3"),
    "fuel_lvl_src": ("Off", "Analog sensor", "Synthetic", "Serial"),
}

# Source parameter -> firmware value that selects the Serial source
SERIAL_SOURCE_VALUES = {
    "speed_source": 6,
    "rpm_source": 4,
    "oil_prs_source": 6,
    "coolant_src": 4,
    "fuel_lvl_src": 3,
}

CAN_PROTOCOL_OPTIONS = ("Haltech v2", "Megasquirt", "AiM", "OBDII")
# Switec Z27.168: 3 steps per degree, driven with 4 microsteps per step
MOTOR_STEPS_PER_DEGREE = 3 * 4
MOTOR_MAX_ANGLE = 315  # Switec Z27.168 mechanical limit, degrees
MOTOR_SWEEP_STEPS_RANGE = (100, MOTOR_MAX_ANGLE * MOTOR_STEPS_PER_DEGREE)
UNIT_SYSTEM_OPTIONS = ("Metric", "'Merican")
# Order must match the MOTOR_FUNC_* values in firmware config_calibration.h
MOTOR_FUNCTION_OPTIONS = ("Off", "Fuel level", "Coolant temp")
MOTOR_FUNCTION_PARAMS = ("m1_func", "m2_func", "m3_func", "m4_func")
# Gauge min/max units per MOTOR_FUNC_* value, and the min/max applied when a function is picked
MOTOR_FUNCTION_UNITS = ("", "%", "\u00b0C")
MOTOR_FUNCTION_RANGES = ((0, 0), (10, 100), (50, 115))
# (function, min, max) parameter names for each gauge motor. CW stores low in min and high in max;
# CCW stores them swapped, which reverses the needle's direction of travel.
MOTOR_PARAMS = (
    ("m1_func", "m1_min", "m1_max"),
    ("m2_func", "m2_min", "m2_max"),
    ("m3_func", "m3_min", "m3_max"),
    ("m4_func", "m4_min", "m4_max"),
)
DIRECTION_OPTIONS = ("CW", "CCW")


def _gauge_range_to_device(low: str, high: str, ccw: bool) -> tuple:
    """Map the user's low/high values to the (min, max) stored in the config."""
    return (high, low) if ccw else (low, high)


def _gauge_range_from_device(min_val: str, max_val: str) -> tuple:
    """Return (low, high, ccw) for stored (min, max) values."""
    try:
        if int(min_val) > int(max_val):
            return max_val, min_val, True
    except ValueError:
        pass
    return min_val, max_val, False

SOURCE_DEPENDENT_PARAMS: Dict[str, tuple] = {
    "revs_per_km": (("speed_source", 2),),
    "teeth_per_rev": (("speed_source", 2),),
    "filter_hall": (("speed_source", 2),),
    "hall_speed_min": (("speed_source", 2),),
    "cyl_count": (("rpm_source", 2),),
    "filter_rpm": (("rpm_source", 2),),
    "rpm_debounce_us": (("rpm_source", 2),),
    "engine_rpm_min": (("rpm_source", 2),),
    "filter_fuel": (("fuel_lvl_src", 1),),
    "filter_therm": (("coolant_src", 2), ("oil_temp_src", 2)),
    "filter_av1": (
        ("oil_prs_source", 2), ("fuel_prs_source", 2),
        ("map_source", 2), ("lambda_source", 2),
    ),
    "filter_av2": (
        ("oil_prs_source", 3), ("fuel_prs_source", 3),
        ("map_source", 3), ("lambda_source", 3),
    ),
    "filter_av3": (
        ("oil_prs_source", 4), ("fuel_prs_source", 4),
        ("map_source", 4), ("lambda_source", 4),
    ),
}

SPEEDO_MAX_UNITS = ("mph", "km/h")
OIL_WARNING_UNITS = ("PSI", "kPa")
TEMPERATURE_UNITS = ("F", "C")
FUEL_CAPACITY_UNITS = ("gal", "L")
CONFIG_FILE_FORMAT = 5
KMH_PER_MPH = 1.609344
PSI_PER_KPA = 0.1450377377
LITERS_PER_GALLON = 3.785411784


def _source_uses_parameter(param: str, selected_sources: Dict[str, str]) -> bool:
    if param in ("speedo_max", "speedo_max_units"):
        try:
            return int(selected_sources.get("speed_source", "")) != 0
        except ValueError:
            return False
    requirements = SOURCE_DEPENDENT_PARAMS.get(param)
    if requirements is None:
        return True
    return any(selected_sources.get(source) == str(value) for source, value in requirements)


def _display_number(value: float) -> str:
    return f"{value:.2f}".rstrip("0").rstrip(".")


def _hall_speed_min_to_raw(value: str) -> int:
    try:
        speed = float(value)
    except ValueError as exc:
        raise ValueError("Minimum Speed Cutoff must be a number from 0 to 1 km/h.") from exc
    if not math.isfinite(speed) or not 0 <= speed <= 1:
        raise ValueError("Minimum Speed Cutoff must be from 0 to 1 km/h.")
    return round(speed * 100)


def _speedo_max_to_raw(value: str, units: str) -> int:
    try:
        speed = float(value)
    except ValueError as exc:
        raise ValueError("Speedometer Max must be a number.") from exc
    if not math.isfinite(speed) or speed <= 0:
        raise ValueError("Speedometer Max must be a positive number.")
    if units == "km/h":
        speed /= KMH_PER_MPH
    elif units != "mph":
        raise ValueError(f"Unsupported speedometer unit: {units}")
    raw = round(speed * 100)
    if not 1000 <= raw <= 30000:
        raise ValueError("Speedometer Max must be between 10 and 300 mph (or the equivalent in km/h).")
    return raw


def _oil_warning_to_raw(value: str, units: str) -> float:
    try:
        pressure = float(value)
    except ValueError as exc:
        raise ValueError("Oil pressure warning must be a number.") from exc
    if not math.isfinite(pressure):
        raise ValueError("Oil pressure warning must be a finite number.")
    if units == "PSI":
        pressure /= PSI_PER_KPA
    elif units != "kPa":
        raise ValueError(f"Unsupported oil pressure unit: {units}")
    if not 0 <= pressure <= 1000:
        raise ValueError("Oil pressure warning must be between 0 and 1000 kPa.")
    return pressure


def _temperature_warning_to_raw(value: str, units: str) -> float:
    try:
        temperature = float(value)
    except ValueError as exc:
        raise ValueError("Engine temperature warning must be a number.") from exc
    if not math.isfinite(temperature):
        raise ValueError("Engine temperature warning must be a finite number.")
    if units == "F":
        temperature = (temperature - 32) * 5 / 9
    elif units != "C":
        raise ValueError(f"Unsupported temperature unit: {units}")
    if not 50 <= temperature <= 200:
        raise ValueError("Engine temperature warning must be between 50 and 200 C.")
    return temperature


def _warning_display_value(raw_value: str, param: str, units: str) -> str:
    value = float(raw_value)
    if param == "oil_warn_kpa" and units == "PSI":
        value *= PSI_PER_KPA
    elif param == "coolant_warn_c" and units == "F":
        value = value * 9 / 5 + 32
    return _display_number(value)


def _angle_to_steps(value: str) -> int:
    try:
        angle = float(value)
    except ValueError as exc:
        raise ValueError("Sweep angle must be a number.") from exc
    if not math.isfinite(angle):
        raise ValueError("Sweep angle must be a finite number.")
    steps = round(angle * MOTOR_STEPS_PER_DEGREE)
    if not MOTOR_SWEEP_STEPS_RANGE[0] <= steps <= MOTOR_SWEEP_STEPS_RANGE[1]:
        lo = MOTOR_SWEEP_STEPS_RANGE[0] / MOTOR_STEPS_PER_DEGREE
        raise ValueError(f"Sweep angle must be between {lo:.1f} and {MOTOR_MAX_ANGLE} degrees.")
    return steps


def _fuel_capacity_to_raw(value: str, units: str) -> float:
    try:
        capacity = float(value)
    except ValueError as exc:
        raise ValueError("Fuel tank capacity must be a number.") from exc
    if not math.isfinite(capacity):
        raise ValueError("Fuel tank capacity must be a finite number.")
    if units == "L":
        capacity /= LITERS_PER_GALLON
    elif units != "gal":
        raise ValueError(f"Unsupported fuel capacity unit: {units}")
    if not 1 <= capacity <= 200:
        raise ValueError("Fuel tank capacity must be between 1 and 200 gallons (or the equivalent in liters).")
    return capacity


def _fuel_capacity_display_value(raw_value: str, units: str) -> str:
    capacity = float(raw_value)
    if units == "L":
        capacity *= LITERS_PER_GALLON
    elif units != "gal":
        raise ValueError(f"Unsupported fuel capacity unit: {units}")
    return _display_number(capacity)


def _decode_config_units(params: Dict[str, str]) -> tuple[Dict[str, str], Dict[str, str]]:
    version = params.get("config_format")
    if version is None:
        return {}, {
            "speedo_max_units": "mph",
            "oil_warn_units": "kPa",
            "coolant_warn_units": "C",
            "fuel_capacity_units": "gal",
        }
    if version not in ("2", "3", "4", str(CONFIG_FILE_FORMAT)):
        raise ValueError(f"Unsupported config file format: {version}")

    units = {
        "speedo_max_units": params.get("speedo_max_units", "mph"),
        "oil_warn_units": params.get("oil_warn_units", "kPa"),
        "coolant_warn_units": params.get("coolant_warn_units", "C"),
        "fuel_capacity_units": params.get("fuel_capacity_units", "gal"),
    }
    if units["speedo_max_units"] not in SPEEDO_MAX_UNITS:
        raise ValueError(f"Unsupported speedometer unit: {units['speedo_max_units']}")
    if units["oil_warn_units"] not in OIL_WARNING_UNITS:
        raise ValueError(f"Unsupported oil pressure unit: {units['oil_warn_units']}")
    if units["coolant_warn_units"] not in TEMPERATURE_UNITS:
        raise ValueError(f"Unsupported engine temperature unit: {units['coolant_warn_units']}")
    if units["fuel_capacity_units"] not in FUEL_CAPACITY_UNITS:
        raise ValueError(f"Unsupported fuel capacity unit: {units['fuel_capacity_units']}")

    converted: Dict[str, str] = {}
    if version in ("2", "3", "4"):
        if "hall_speed_min" in params:
            converted["hall_speed_min"] = str(_hall_speed_min_to_raw(params["hall_speed_min"]))
        if "speedo_max" in params:
            converted["speedo_max"] = str(
                _speedo_max_to_raw(params["speedo_max"], units["speedo_max_units"])
            )
    if version in ("3", "4"):
        if "oil_warn_kpa" in params:
            converted["oil_warn_kpa"] = str(
                _oil_warning_to_raw(params["oil_warn_kpa"], units["oil_warn_units"])
            )
        if "coolant_warn_c" in params:
            converted["coolant_warn_c"] = str(
                _temperature_warning_to_raw(
                    params["coolant_warn_c"], units["coolant_warn_units"],
                )
            )
    if version == "4" and "fuel_capacity" in params:
        converted["fuel_capacity"] = str(
            _fuel_capacity_to_raw(params["fuel_capacity"], units["fuel_capacity_units"])
        )
    if version != "5":
        units["fuel_capacity_units"] = "gal"
    if version == "2":
        units["oil_warn_units"] = "kPa"
        units["coolant_warn_units"] = "C"
    return converted, units


# Milliseconds to let the freshly flashed sketch start before re-opening the port.
BOOTLOADER_EXIT_MS = 3000

ALL_PARAMS = [
    "m1_sweep", "m2_sweep", "m3_sweep", "m4_sweep", "ms_sweep",
    "m1_func", "m2_func", "m3_func", "m4_func",
    "m1_min", "m1_max", "m2_min", "m2_max", "m3_min", "m3_max", "m4_min", "m4_max",
    "ms_zero_delay", "ms_zero_factor", "motor_sweep_ms",
    "filter_vbatt", "vbatt_scaler", "filter_fuel", "filter_therm",
    "filter_av1", "filter_av2", "filter_av3",
    "revs_per_km", "teeth_per_rev", "filter_hall", "hall_speed_min",
    "cyl_count", "filter_rpm", "rpm_debounce_us", "engine_rpm_min",
    "speedo_max",
    "num_leds", "warn_leds", "shift_leds", "tach_max", "tach_min",
    "odo_steps", "odo_motor_teeth", "odo_gear_teeth",
    "speed_source", "rpm_source", "oil_prs_source", "fuel_prs_source",
    "coolant_src", "oil_temp_src", "map_source", "lambda_source", "fuel_lvl_src",
    "oil_warn_kpa", "coolant_warn_c", "batt_warn_v", "engine_run_rpm", "fuel_warn_pct",
    "clock_offset", "fuel_capacity", "can_protocol", "units",
    "disp1_order", "disp2_order",
]

TOOLTIPS: Dict[str, str] = {
    "m1_sweep":          "Motor 1 full sweep in steps (100-4095). 58 x 12 = 696.",
    "m2_sweep":          "Motor 2 full sweep in steps (100-4095).",
    "m3_sweep":          "Motor 3 full sweep in steps (100-4095).",
    "m4_sweep":          "Motor 4 full sweep in steps (100-4095).",
    "ms_sweep":          "Motor S (speedometer NEMA14) sweep steps (100-16000).",
    "m1_func":           "Signal shown by Motor 1 (Off parks the needle at zero).",
    "m2_func":           "Signal shown by Motor 2 (Off parks the needle at zero).",
    "m3_func":           "Signal shown by Motor 3 (Off parks the needle at zero).",
    "m4_func":           "Signal shown by Motor 4 (Off parks the needle at zero).",
    "m1_min":            "Motor 1 gauge value at the zero end of the dial (fuel: %, coolant: C).",
    "m1_max":            "Motor 1 gauge value at the full end of the dial (fuel: %, coolant: C).",
    "m2_min":            "Motor 2 gauge value at the zero end of the dial (fuel: %, coolant: C).",
    "m2_max":            "Motor 2 gauge value at the full end of the dial (fuel: %, coolant: C).",
    "m3_min":            "Motor 3 gauge value at the zero end of the dial (fuel: %, coolant: C).",
    "m3_max":            "Motor 3 gauge value at the full end of the dial (fuel: %, coolant: C).",
    "m4_min":            "Motor 4 gauge value at the zero end of the dial (fuel: %, coolant: C).",
    "m4_max":            "Motor 4 gauge value at the full end of the dial (fuel: %, coolant: C).",
    "motor_direction":   "Direction the needle sweeps as the value rises. CCW swaps the min and max stored in the config.",
    "ms_zero_delay":     "Motor S zeroing step delay us (10-5000).",
    "ms_zero_factor":    "Fraction of MS_SWEEP for Motor S zeroing (0.1-1.0).",
    "motor_sweep_ms":    "Startup test sweep duration ms (100-5000).",
    "filter_vbatt":      "Battery voltage EMA filter coefficient (1-255). 255=no filter.",
    "vbatt_scaler":      "Voltage divider scale factor for battery ADC (0.001-0.1).",
    "filter_fuel":       "Fuel sensor EMA filter coefficient (1-255).",
    "filter_therm":      "Thermistor EMA filter coefficient (1-255).",
    "filter_av1":        "AV1 sensor EMA filter coefficient (1-255).",
    "filter_av2":        "AV2 sensor EMA filter coefficient (1-255).",
    "filter_av3":        "AV3 sensor EMA filter coefficient (1-255).",
    "revs_per_km":       "VSS shaft revolutions per km (100-10000).",
    "teeth_per_rev":     "VSS teeth per shaft revolution (1-64).",
    "filter_hall":       "Hall speed EMA filter coefficient (1-255).",
    "hall_speed_min":    "Minimum speed cutoff in km/h (0-1).",
    "cyl_count":         "Engine cylinder count (2-16).",
    "filter_rpm":        "RPM EMA filter coefficient (1-255).",
    "rpm_debounce_us":   "RPM pulse debounce window us (100-20000).",
    "engine_rpm_min":    "Minimum displayed RPM (0-255).",
    "speedo_max":        "Maximum speedometer reading in the selected unit.",
    "num_leds":          "Total LED strip count (1-64).",
    "warn_leds":         "Warning zone LEDs per side (0-32).",
    "shift_leds":        "Shift-light LEDs per side (0-16).",
    "tach_max":          "Shift RPM (1000-15000).",
    "tach_min":          "RPM at which first LED is active",
    "odo_steps":         "Odometer motor steps per revolution (512-8192).",
    "odo_motor_teeth":   "Odometer motor gear tooth count (1-64).",
    "odo_gear_teeth":    "Odometer driven gear tooth count (1-64).",
    "speed_source":      "Select where the speed signal comes from.",
    "rpm_source":        "Select where the RPM signal comes from.",
    "oil_prs_source":    "Select where the oil pressure signal comes from.",
    "fuel_prs_source":   "Select where the fuel pressure signal comes from.",
    "coolant_src":       "Select where the coolant temperature signal comes from.",
    "oil_temp_src":      "Select where the oil temperature signal comes from.",
    "map_source":        "Select where the MAP / boost signal comes from.",
    "lambda_source":     "Select where the lambda / AFR signal comes from.",
    "fuel_lvl_src":      "Select where the fuel level signal comes from.",
    "oil_warn_kpa":      "Oil pressure fault threshold (0-1000 kPa or equivalent PSI). Flash below this.",
    "coolant_warn_c":    "Engine temperature warning threshold (50-200 C or equivalent F). Flash above this.",
    "batt_warn_v":       "Battery voltage fault threshold V (5-16). Flash below this.",
    "engine_run_rpm":    "Min RPM to consider engine running for fault logic (0-2000).",
    "fuel_warn_pct":     "Low fuel warning threshold % (0-50). Flash below this.",
    "clock_offset":      "UTC offset hours (-12 to +12).",
    "fuel_capacity":     "Fuel tank capacity in gallons or liters (1-200 gallons).",
    "can_protocol":      "Select the CAN protocol used by your vehicle.",
    "units":             "Unit system shown on the gauge: Metric or 'Merican (imperial).",
    "disp1_order":       "Screens reachable by scrolling display 1, in order (Settings is always last).",
    "disp2_order":       "Screens selectable for display 2 from the Settings menu, in order.",
}

SCREEN_NAMES = [
    (1, "Oil Pressure"), (2, "Coolant Temp"), (3, "Fuel Level"),
    (4, "Battery Voltage"), (5, "Engine RPM"), (6, "Vehicle Speed"),
    (7, "Air/Fuel Ratio"), (8, "Fuel Pressure"), (9, "Boost (bar)"),
    (10, "Boost (text)"), (11, "Oil Temp"), (12, "Fuel Composition"),
    (15, "Trip Odometer"),
    (16, "Clock"), (17, "Falcon Script logo"),
]


def parse_screen_order(text: str) -> list:
    """Parse '1,5,3' into a list of unique valid screen IDs (invalid text -> [])."""
    valid = {sid for sid, _ in SCREEN_NAMES}
    result: list = []
    for part in text.split(","):
        part = part.strip()
        if part.isdigit() and int(part) in valid and int(part) not in result:
            result.append(int(part))
    return result


class ScreenListEditor(ttk.Frame):
    """Checkbox list of screens that can be reordered by dragging rows.

    The bound StringVar holds the checked screen IDs in display order,
    e.g. "1,5,3".  An empty var shows every screen checked in default order.
    """

    def __init__(self, parent: tk.Widget, variable: tk.StringVar) -> None:
        super().__init__(parent)
        self._var = variable
        self._names = dict(SCREEN_NAMES)
        self._order = [sid for sid, _ in SCREEN_NAMES]
        self._checked = {sid: tk.BooleanVar(value=True) for sid in self._order}
        self._rows: Dict[int, tk.Frame] = {}
        self._drag_id: Optional[int] = None
        self._syncing = False
        for sid in self._order:
            row = tk.Frame(self, borderwidth=1, relief="flat")
            ttk.Checkbutton(
                row, variable=self._checked[sid],
                command=lambda s=sid: self._on_toggle(s),
            ).pack(side="left")
            for text in ("\u2630", self._names[sid]):
                lbl = ttk.Label(row, text=text, cursor="fleur")
                lbl.pack(side="left", padx=(0, 6))
                lbl.bind("<ButtonPress-1>", lambda _e, s=sid: self._drag_start(s))
                lbl.bind("<B1-Motion>", self._drag_motion)
                lbl.bind("<ButtonRelease-1>", self._drag_end)
            self._rows[sid] = row
        self._var.trace_add("write", lambda *_a: self._load_from_var())
        self._load_from_var()

    def _layout(self) -> None:
        for i, sid in enumerate(self._order):
            self._rows[sid].grid(row=i, column=0, sticky="we", pady=1)

    def _load_from_var(self) -> None:
        if self._syncing:
            return
        enabled = parse_screen_order(self._var.get())
        if enabled:
            rest = [sid for sid in self._order if sid not in enabled]
            self._order = enabled + rest
        else:
            self._order = [sid for sid, _ in SCREEN_NAMES]
        for sid, var in self._checked.items():
            var.set(sid in enabled if enabled else True)
        self._layout()

    def _commit(self) -> None:
        value = ",".join(str(s) for s in self._order if self._checked[s].get())
        self._syncing = True
        try:
            self._var.set(value)
        finally:
            self._syncing = False

    def _on_toggle(self, sid: int) -> None:
        if not any(v.get() for v in self._checked.values()):
            self._checked[sid].set(True)  # at least one screen must stay enabled
            self.bell()
            return
        self._commit()

    def _drag_start(self, sid: int) -> None:
        self._drag_id = sid
        self._rows[sid].configure(relief="sunken")

    def _drag_motion(self, event: tk.Event) -> None:
        if self._drag_id is None:
            return
        y = event.y_root - self.winfo_rooty()
        row_h = max(self._rows[self._drag_id].winfo_height() + 2, 1)
        target = max(0, min(len(self._order) - 1, y // row_h))
        current = self._order.index(self._drag_id)
        if target != current:
            self._order.insert(target, self._order.pop(current))
            self._layout()

    def _drag_end(self, _event: object) -> None:
        if self._drag_id is None:
            return
        self._rows[self._drag_id].configure(relief="flat")
        self._drag_id = None
        self._commit()


class ToolTip:
    def __init__(self, widget: tk.Widget, text: str) -> None:
        self._widget = widget
        self._text = text
        self._tip: Optional[tk.Toplevel] = None
        widget.bind("<Enter>", self._show)
        widget.bind("<Leave>", self._hide)

    def _show(self, _event: object) -> None:
        x = self._widget.winfo_rootx() + 20
        y = self._widget.winfo_rooty() + 20
        self._tip = tk.Toplevel(self._widget)
        self._tip.wm_overrideredirect(True)
        self._tip.wm_geometry(f"+{x}+{y}")
        lbl = tk.Label(
            self._tip, text=self._text, background="#ffffe0",
            relief="solid", borderwidth=1, font=("TkDefaultFont", 9),
            wraplength=300, justify="left",
        )
        lbl.pack()

    def _hide(self, _event: object) -> None:
        if self._tip:
            self._tip.destroy()
            self._tip = None


class GaugeConfigApp:
    """Root application window with 11 tabbed sections."""

    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title(WINDOW_TITLE)
        self.root.minsize(800, 600)

        self._serial: Optional[GaugeSerial] = GaugeSerial() if HAS_SERIAL else None  # type: ignore[misc]
        self._config_file = ConfigFile()
        self._vars: Dict[str, tk.StringVar] = {p: tk.StringVar(value="") for p in ALL_PARAMS}
        self._dirty: Dict[str, bool] = {p: False for p in ALL_PARAMS}
        self._gauge_display_vars: Dict[str, tuple] = {}
        self._hall_speed_min_display_var = tk.StringVar(value="")
        self._speedo_max_display_var = tk.StringVar(value="")
        self._angle_vars: Dict[str, tk.StringVar] = {}
        self._syncing_angle = False
        self._speedo_max_units_var = tk.StringVar(value="mph")
        self._syncing_speed_display = False
        self._speed_display_widgets: Dict[str, ttk.Entry] = {}
        self._oil_warn_display_var = tk.StringVar(value="")
        self._oil_warn_units_var = tk.StringVar(value="kPa")
        self._coolant_warn_display_var = tk.StringVar(value="")
        self._coolant_warn_units_var = tk.StringVar(value="C")
        self._syncing_warning_display = False
        self._fuel_capacity_display_var = tk.StringVar(value="")
        self._fuel_capacity_units_var = tk.StringVar(value="gal")
        self._syncing_fuel_capacity = False
        self._param_widgets: Dict[str, ttk.Widget] = {}
        self._param_labels: Dict[str, ttk.Label] = {}
        self._source_dependent_widgets: Dict[str, list[ttk.Widget]] = {}
        self._splash_data: Dict[int, bytes] = {}
        self._port_devices: Dict[str, str] = {}

        # Firmware flashing runs on a worker thread; it reports back here.
        self._fw_queue: "queue.Queue[tuple]" = queue.Queue()
        self._fw_thread: Optional[threading.Thread] = None
        self._fw_was_connected = False

        self._build_menu()
        self._build_toolbar()
        self._build_tabs()
        self._build_status_bar()

    def _build_menu(self) -> None:
        mb = tk.Menu(self.root)
        file_menu = tk.Menu(mb, tearoff=False)
        file_menu.add_command(label="Save config to file...", command=self._save_to_file)
        file_menu.add_command(label="Load config from file...", command=self._load_from_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        mb.add_cascade(label="File", menu=file_menu)
        dev_menu = tk.Menu(mb, tearoff=False)
        dev_menu.add_command(label="Read from device", command=self._read_from_device)
        dev_menu.add_command(label="Write to device", command=self._write_to_device)
        dev_menu.add_separator()
        dev_menu.add_command(label="Reset to defaults", command=self._reset_defaults)
        mb.add_cascade(label="Device", menu=dev_menu)
        self.root.config(menu=mb)

    def _build_toolbar(self) -> None:
        bar = ttk.Frame(self.root, relief="raised")
        bar.pack(side="top", fill="x", pady=2)
        ttk.Button(bar, text="Read from device", command=self._read_from_device).pack(side="left", padx=4, pady=2)
        ttk.Button(bar, text="Write to device",  command=self._write_to_device).pack(side="left", padx=4, pady=2)
        ttk.Button(bar, text="Save to file...",  command=self._save_to_file).pack(side="left", padx=4, pady=2)
        ttk.Button(bar, text="Load from file...", command=self._load_from_file).pack(side="left", padx=4, pady=2)

    def _build_status_bar(self) -> None:
        self._status_var = tk.StringVar(value="Disconnected")
        bar = ttk.Label(self.root, textvariable=self._status_var, relief="sunken", anchor="w")
        bar.pack(side="bottom", fill="x")

    def _set_status(self, msg: str) -> None:
        self._status_var.set(msg)

    def _build_tabs(self) -> None:
        nb = ttk.Notebook(self.root)
        self._notebook = nb
        nb.pack(fill="both", expand=True, padx=4, pady=4)
        tabs = [
            ("Connection",       self._build_tab_connection),
            ("Motor Config",     self._build_tab_motors),
            ("Signal Source",    self._build_tab_signal_source),
            ("Speed/RPM",        self._build_tab_speed_rpm),
            ("Sensors",          self._build_tab_sensors),
            ("LED Tach",         self._build_tab_led_tach),
            ("Display Settings", self._build_tab_display),
            ("Fault Warnings",   self._build_tab_faults),
            ("Splash Screens",   self._build_tab_splash),
            ("Test",             self._build_tab_test),
        ]
        for title, builder in tabs:
            frame = ttk.Frame(nb)
            nb.add(frame, text=title)
            builder(frame)
        self._update_source_dependent_controls()
        nb.bind("<<NotebookTabChanged>>", self._on_tab_changed)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _on_tab_changed(self, _event: object = None) -> None:
        if self._notebook.tab(self._notebook.select(), "text") != "Test":
            self._test_stop_odo_ui()
            self._test_restore_sources()

    def _on_close(self) -> None:
        self._test_restore_sources()
        self.root.destroy()

    def _param_row(self, parent: tk.Widget, row: int, col: int,
                   label: str, param: str, width: int = 12) -> ttk.Entry:
        lbl = ttk.Label(parent, text=label)
        lbl.grid(row=row, column=col, sticky="e", padx=4, pady=2)
        self._param_labels[param] = lbl
        options = SOURCE_OPTIONS.get(param)
        if param == "can_protocol":
            options = CAN_PROTOCOL_OPTIONS
        elif param == "units":
            options = UNIT_SYSTEM_OPTIONS
        elif param in MOTOR_FUNCTION_PARAMS:
            options = MOTOR_FUNCTION_OPTIONS
        if options is not None:
            display_var = tk.StringVar()
            widget = ttk.Combobox(
                parent, textvariable=display_var, values=options,
                width=max(width, max(map(len, options))), state="readonly",
            )
            widget.bind(
                "<<ComboboxSelected>>",
                lambda _event, p=param, combo=widget: self._vars[p].set(str(combo.current())),
            )

            def update_source_display(*_args: object) -> None:
                try:
                    value = int(self._vars[param].get())
                    display_var.set(options[value] if 0 <= value < len(options) else "Unsupported source")
                except ValueError:
                    display_var.set("Unsupported source" if self._vars[param].get() else "")

            self._vars[param].trace_add("write", update_source_display)
            update_source_display()
        else:
            widget = ttk.Entry(parent, textvariable=self._vars[param], width=width)
        widget.grid(row=row, column=col + 1, sticky="w", padx=4, pady=2)
        self._param_widgets[param] = widget
        if param in ("tach_max", "tach_min"):
            widget.bind("<FocusOut>", lambda _event: self._enforce_tach_min())
        if param in TOOLTIPS:
            ToolTip(lbl,   TOOLTIPS[param])
            ToolTip(widget, TOOLTIPS[param])

        def on_change(*_args: object) -> None:
            self._dirty[param] = True
            widget.configure(foreground="blue")

        self._vars[param].trace_add("write", on_change)
        if param in SOURCE_OPTIONS:
            self._vars[param].trace_add(
                "write", lambda *_args: self._update_source_dependent_controls(),
            )
        return widget

    def _warning_threshold_row(
        self,
        parent: tk.Widget,
        row: int,
        label: str,
        param: str,
        units: tuple,
        display_var: tk.StringVar,
        units_var: tk.StringVar,
    ) -> None:
        lbl = ttk.Label(parent, text=label)
        lbl.grid(row=row, column=0, sticky="e", padx=4, pady=2)
        entry = ttk.Entry(parent, textvariable=display_var, width=12)
        entry.grid(row=row, column=1, sticky="w", padx=4, pady=2)
        self._param_labels[param] = lbl
        self._param_widgets[param] = entry
        ToolTip(lbl, TOOLTIPS[param])
        ToolTip(entry, TOOLTIPS[param])
        display_var.trace_add(
            "write", lambda *_args, p=param: self._on_warning_display_changed(p),
        )
        self._vars[param].trace_add(
            "write", lambda *_args, p=param: self._sync_warning_display(p),
        )
        self._vars[param].trace_add(
            "write",
            lambda *_args, p=param: self._mark_parameter_dirty(p),
        )
        self._unit_radio_group(
            parent, row, 2, units_var, units,
            lambda p=param: self._warning_units_changed(p),
        )
        self._sync_warning_display(param)

    @staticmethod
    def _unit_radio_group(
        parent: tk.Widget,
        row: int,
        column: int,
        units_var: tk.StringVar,
        units: tuple,
        command: Callable[[], None],
    ) -> list[ttk.Radiobutton]:
        group = ttk.Frame(parent)
        group.grid(row=row, column=column, columnspan=2, sticky="w", padx=4, pady=2)
        radios = []
        for unit in units:
            radio = ttk.Radiobutton(
                group, text=unit, variable=units_var, value=unit,
                command=command,
            )
            radio.pack(side="left", padx=(0, 8))
            radios.append(radio)
        return radios

    def _mark_parameter_dirty(self, param: str) -> None:
        self._dirty[param] = True
        widget = self._param_widgets.get(param)
        if widget:
            widget.configure(foreground="blue")

    def _selected_sources(self) -> Dict[str, str]:
        return {
            param: self._vars[param].get()
            for param in SOURCE_OPTIONS
        }

    def _set_source_dependent_state(self, param: str, active: bool) -> None:
        state = "!disabled" if active else "disabled"
        widget = self._param_widgets.get(param)
        if widget:
            widget.state([state])
        label = self._param_labels.get(param)
        if label:
            label.state([state])
        for dependent in self._source_dependent_widgets.get(param, []):
            dependent.state([state])

    def _update_source_dependent_controls(self) -> None:
        selected_sources = self._selected_sources()
        dependent_params = set(SOURCE_DEPENDENT_PARAMS)
        dependent_params.add("speedo_max")
        for param in dependent_params:
            self._set_source_dependent_state(
                param, _source_uses_parameter(param, selected_sources),
            )

    def _enforce_tach_min(self) -> None:
        try:
            shift_rpm = int(self._vars["tach_max"].get())
            min_display_rpm = int(self._vars["tach_min"].get())
        except ValueError:
            return
        if min_display_rpm > shift_rpm:
            self._vars["tach_min"].set(str(shift_rpm))

    def _sync_speed_display(self, param: str) -> None:
        if self._syncing_speed_display:
            return
        try:
            raw = int(self._vars[param].get())
        except ValueError:
            return
        if param == "hall_speed_min":
            value = raw / 100
            display_var = self._hall_speed_min_display_var
        else:
            value = raw / 100
            if self._speedo_max_units_var.get() == "km/h":
                value *= KMH_PER_MPH
            display_var = self._speedo_max_display_var
        self._syncing_speed_display = True
        try:
            display_var.set(_display_number(value))
        finally:
            self._syncing_speed_display = False

    def _commit_speed_display(self, param: str) -> None:
        self._on_speed_display_changed(param)
        try:
            if param == "hall_speed_min":
                _hall_speed_min_to_raw(self._hall_speed_min_display_var.get())
            else:
                _speedo_max_to_raw(
                    self._speedo_max_display_var.get(),
                    self._speedo_max_units_var.get(),
                )
        except ValueError:
            return
        self._sync_speed_display(param)

    def _on_speed_display_changed(self, param: str) -> None:
        if self._syncing_speed_display:
            return
        self._dirty[param] = True
        widget = self._speed_display_widgets.get(param)
        if widget:
            widget.configure(foreground="blue")
        try:
            if param == "hall_speed_min":
                raw = _hall_speed_min_to_raw(self._hall_speed_min_display_var.get())
            else:
                raw = _speedo_max_to_raw(
                    self._speedo_max_display_var.get(),
                    self._speedo_max_units_var.get(),
                )
        except ValueError:
            return
        self._syncing_speed_display = True
        try:
            self._vars[param].set(str(raw))
        finally:
            self._syncing_speed_display = False

    def _speedo_max_units_changed(self) -> None:
        self._sync_speed_display("speedo_max")

    def _validate_speed_inputs(self) -> bool:
        try:
            sources = self._selected_sources()
            hall_min = None
            if _source_uses_parameter("hall_speed_min", sources):
                hall_value = self._hall_speed_min_display_var.get()
                if hall_value:
                    hall_min = _hall_speed_min_to_raw(hall_value)
                elif self._dirty["hall_speed_min"]:
                    raise ValueError("Minimum Speed Cutoff must be a number from 0 to 1 km/h.")
            speedo_max = None
            if _source_uses_parameter("speedo_max", sources):
                speedo_value = self._speedo_max_display_var.get()
                if speedo_value:
                    speedo_max = _speedo_max_to_raw(
                        speedo_value, self._speedo_max_units_var.get(),
                    )
                elif self._dirty["speedo_max"]:
                    raise ValueError("Speedometer Max must be a number.")
        except ValueError as exc:
            messagebox.showerror("Invalid speed setting", str(exc))
            return False
        if hall_min is not None and self._vars["hall_speed_min"].get() != str(hall_min):
            self._vars["hall_speed_min"].set(str(hall_min))
        if speedo_max is not None and self._vars["speedo_max"].get() != str(speedo_max):
            self._vars["speedo_max"].set(str(speedo_max))
        return True

    def _sync_warning_display(self, param: str) -> None:
        if self._syncing_warning_display:
            return
        raw_value = self._vars[param].get()
        if not raw_value:
            return
        if param == "oil_warn_kpa":
            display_var = self._oil_warn_display_var
            units = self._oil_warn_units_var.get()
        else:
            display_var = self._coolant_warn_display_var
            units = self._coolant_warn_units_var.get()
        try:
            display_value = _warning_display_value(raw_value, param, units)
        except ValueError:
            return
        self._syncing_warning_display = True
        try:
            display_var.set(display_value)
        finally:
            self._syncing_warning_display = False

    def _on_warning_display_changed(self, param: str) -> None:
        if self._syncing_warning_display:
            return
        self._dirty[param] = True
        if param == "oil_warn_kpa":
            display_var = self._oil_warn_display_var
            units = self._oil_warn_units_var.get()
        else:
            display_var = self._coolant_warn_display_var
            units = self._coolant_warn_units_var.get()
        try:
            if param == "oil_warn_kpa":
                raw_value = _oil_warning_to_raw(display_var.get(), units)
            else:
                raw_value = _temperature_warning_to_raw(display_var.get(), units)
        except ValueError:
            return
        self._syncing_warning_display = True
        try:
            self._vars[param].set(f"{raw_value:.2f}".rstrip("0").rstrip("."))
        finally:
            self._syncing_warning_display = False

    def _warning_units_changed(self, param: str) -> None:
        self._sync_warning_display(param)

    def _validate_warning_inputs(self) -> bool:
        try:
            warning_values = (
                ("oil_warn_kpa", self._oil_warn_display_var.get(), self._oil_warn_units_var.get()),
                ("coolant_warn_c", self._coolant_warn_display_var.get(), self._coolant_warn_units_var.get()),
            )
            for param, display_value, units in warning_values:
                if not display_value:
                    if self._dirty[param]:
                        raise ValueError(f"{param} must be a number.")
                    continue
                raw_value = (
                    _oil_warning_to_raw(display_value, units)
                    if param == "oil_warn_kpa"
                    else _temperature_warning_to_raw(display_value, units)
                )
                raw_text = f"{raw_value:.2f}".rstrip("0").rstrip(".")
                if self._vars[param].get() != raw_text:
                    self._vars[param].set(raw_text)
        except ValueError as exc:
            messagebox.showerror("Invalid warning threshold", str(exc))
            return False
        return True

    def _sync_fuel_capacity_display(self) -> None:
        if self._syncing_fuel_capacity:
            return
        raw_value = self._vars["fuel_capacity"].get()
        if not raw_value:
            return
        try:
            display_value = _fuel_capacity_display_value(
                raw_value, self._fuel_capacity_units_var.get(),
            )
        except ValueError:
            return
        self._syncing_fuel_capacity = True
        try:
            self._fuel_capacity_display_var.set(display_value)
        finally:
            self._syncing_fuel_capacity = False

    def _on_fuel_capacity_display_changed(self) -> None:
        if self._syncing_fuel_capacity:
            return
        self._dirty["fuel_capacity"] = True
        try:
            raw_value = _fuel_capacity_to_raw(
                self._fuel_capacity_display_var.get(),
                self._fuel_capacity_units_var.get(),
            )
        except ValueError:
            return
        self._syncing_fuel_capacity = True
        try:
            raw_text = f"{raw_value:.2f}".rstrip("0").rstrip(".")
            if self._vars["fuel_capacity"].get() != raw_text:
                self._vars["fuel_capacity"].set(raw_text)
        finally:
            self._syncing_fuel_capacity = False

    def _fuel_capacity_units_changed(self) -> None:
        self._sync_fuel_capacity_display()

    def _validate_fuel_capacity(self) -> bool:
        value = self._fuel_capacity_display_var.get()
        if not value:
            if self._dirty["fuel_capacity"]:
                messagebox.showerror("Invalid fuel capacity", "Fuel tank capacity must be a number.")
                return False
            return True
        try:
            raw_value = _fuel_capacity_to_raw(value, self._fuel_capacity_units_var.get())
        except ValueError as exc:
            messagebox.showerror("Invalid fuel capacity", str(exc))
            return False
        raw_text = f"{raw_value:.2f}".rstrip("0").rstrip(".")
        if self._vars["fuel_capacity"].get() != raw_text:
            self._vars["fuel_capacity"].set(raw_text)
        return True

    # ------ Tab 1: Connection ------
    def _build_tab_connection(self, frame: ttk.Frame) -> None:
        inner = ttk.LabelFrame(frame, text="Serial Connection")
        inner.pack(padx=10, pady=10, fill="x")
        ttk.Label(inner, text="Port:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        self._port_var = tk.StringVar()
        self._port_combo = ttk.Combobox(inner, textvariable=self._port_var, width=32)
        self._port_combo.grid(row=0, column=1, sticky="w", padx=4, pady=4)
        ttk.Button(inner, text="Refresh", command=self._refresh_ports).grid(row=0, column=2, padx=4)
        ttk.Label(inner, text="Baud:").grid(row=1, column=0, sticky="e", padx=4, pady=4)
        self._baud_var = tk.StringVar(value="115200")
        ttk.Combobox(inner, textvariable=self._baud_var, width=10,
                     values=["9600", "57600", "115200"]).grid(row=1, column=1, sticky="w", padx=4, pady=4)
        self._connect_btn = ttk.Button(inner, text="Connect", command=self._toggle_connection)
        self._connect_btn.grid(row=2, column=0, columnspan=2, pady=8)
        ttk.Label(inner, text="Status:").grid(row=3, column=0, sticky="e", padx=4)
        self._conn_status_var = tk.StringVar(value="Disconnected")
        ttk.Label(inner, textvariable=self._conn_status_var).grid(row=3, column=1, sticky="w", padx=4)
        ttk.Label(inner, text="Firmware:").grid(row=4, column=0, sticky="e", padx=4)
        self._fw_var = tk.StringVar(value="--")
        ttk.Label(inner, textvariable=self._fw_var).grid(row=4, column=1, sticky="w", padx=4)
        self._build_firmware_group(frame)
        self._refresh_ports()

    # ------ Tab 1: Firmware update (spec §13) ------
    def _build_firmware_group(self, parent: ttk.Frame) -> None:
        """Firmware picker and one-click update. No toolchain required."""
        grp = ttk.LabelFrame(parent, text="Firmware Update")
        grp.pack(padx=10, pady=(0, 10), fill="both", expand=True)
        grp.columnconfigure(1, weight=1)

        ttk.Label(grp, text="Firmware file:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        self._fw_path_var = tk.StringVar()
        path_entry = ttk.Entry(grp, textvariable=self._fw_path_var)
        path_entry.grid(row=0, column=1, sticky="ew", padx=4, pady=4)
        ToolTip(path_entry, "The .hex firmware file supplied with your gauge.")
        ttk.Button(grp, text="Browse...", command=self._fw_browse_binary
                   ).grid(row=0, column=2, padx=4, pady=4)

        actions = ttk.Frame(grp)
        actions.grid(row=1, column=0, columnspan=3, sticky="ew", padx=4, pady=6)
        actions.columnconfigure(1, weight=1)
        self._fw_upload_btn = ttk.Button(actions, text="Update Firmware",
                                         command=self._fw_upload)
        self._fw_upload_btn.grid(row=0, column=0, padx=(0, 8))
        self._fw_pb = ttk.Progressbar(actions, orient="horizontal", mode="determinate")
        self._fw_pb.grid(row=0, column=1, sticky="ew")

        self._fw_log_text = scrolledtext.ScrolledText(grp, height=10, wrap="word",
                                                      state="disabled")
        self._fw_log_text.grid(row=2, column=0, columnspan=3, sticky="nsew", padx=4, pady=(0, 6))
        grp.rowconfigure(2, weight=1)

        if not HAS_FIRMWARE:
            self._fw_upload_btn.state(["disabled"])
            self._fw_log("Firmware update unavailable: pyserial is not installed.")
            return
        self._fw_autoselect()

    def _fw_autoselect(self) -> None:
        """Pre-select a firmware image shipped alongside the tool, if there is one."""
        candidates = firmware_uploader.list_firmware()
        if not candidates:
            self._fw_log("Select the firmware file supplied with your gauge, "
                         "then press Update Firmware.")
            return
        self._fw_path_var.set(candidates[0])
        self._fw_log(f"Found firmware: {os.path.basename(candidates[0])}")
        if len(candidates) > 1:
            self._fw_log(f"({len(candidates) - 1} other .hex file(s) nearby — "
                         "use Browse if this is not the right one.)")
        self._fw_describe(candidates[0])

    def _fw_describe(self, path: str) -> bool:
        """Validate *path* and report it in the log. Returns True if usable."""
        try:
            warnings = firmware_uploader.validate(path)
        except FirmwareError as exc:
            self._fw_log(f"Cannot use this file: {exc}")
            return False
        size = len(firmware_uploader.load_image(path))
        self._fw_log(f"Ready to flash: {size / 1024:.1f} KB.")
        for warning in warnings:
            self._fw_log(f"Note: {warning}")
        return True

    def _fw_log(self, line: str) -> None:
        self._fw_log_text.configure(state="normal")
        self._fw_log_text.insert("end", line + "\n")
        self._fw_log_text.see("end")
        self._fw_log_text.configure(state="disabled")

    def _firmware_initial_dir(self) -> str:
        """Open the file dialog wherever firmware is most likely to be."""
        current = self._fw_path_var.get().strip()
        if current and os.path.isfile(current):
            return os.path.dirname(current)
        dirs = firmware_uploader.firmware_search_dirs() if HAS_FIRMWARE else []
        return dirs[0] if dirs else os.getcwd()

    def _fw_browse_binary(self) -> None:
        path = filedialog.askopenfilename(
            title="Select firmware file",
            initialdir=self._firmware_initial_dir(),
            filetypes=[("Firmware", "*.hex"), ("All files", "*.*")],
        )
        if not path:
            return
        self._fw_path_var.set(path)
        if not HAS_FIRMWARE:
            return
        self._fw_log("")
        self._fw_log(f"Selected {os.path.basename(path)}")
        if self._fw_describe(path):
            self._set_status(f"Firmware selected: {os.path.basename(path)}")

    def _fw_upload(self) -> None:
        """Validate, confirm, drop the serial connection, then flash on a worker thread."""
        if not HAS_FIRMWARE:
            messagebox.showerror("Unavailable",
                                 "pyserial is required. Run: pip install pyserial")
            return
        if self._fw_thread and self._fw_thread.is_alive():
            messagebox.showinfo("Busy", "A firmware update is already running.")
            return

        path = self._fw_path_var.get().strip()
        try:
            warnings = firmware_uploader.validate(path)
        except FirmwareError as exc:
            messagebox.showerror("Cannot use this firmware file", str(exc))
            return

        selected_port = self._port_var.get()
        port = self._port_devices.get(selected_port, selected_port).strip()
        if not port:
            messagebox.showwarning("No port",
                                   "Select the gauge's port first, then press Update Firmware.")
            return

        prompt = [f"Update the firmware on {port}?", "",
                  f"File: {os.path.basename(path)}", "",
                  "The gauge will be unresponsive for about a minute.",
                  "Do not unplug it until this finishes."]
        if warnings:
            prompt += [""] + [f"Note: {w}" for w in warnings]
        if not messagebox.askyesno("Confirm firmware update", "\n".join(prompt)):
            return

        # The bootloader needs exclusive access to the port, and flashing resets
        # the board, so the tool's own connection has to go first.
        self._fw_was_connected = bool(self._serial and self._serial.is_connected)
        if self._fw_was_connected and self._serial:
            self._test_restore_sources()
            self._serial.disconnect()
            self._conn_status_var.set("Disconnected (updating)")
            self._connect_btn.configure(text="Connect")

        self._fw_pb["value"] = 0
        self._fw_log("")
        self._fw_log(f"--- Updating firmware on {port} ---")
        self._fw_upload_btn.state(["disabled"])
        self._connect_btn.state(["disabled"])
        self._set_status("Updating firmware — do not unplug the gauge.")

        uploader = FirmwareUploader()
        args = (uploader, path, port)
        self._fw_thread = threading.Thread(target=self._fw_worker, args=args, daemon=True)
        self._fw_thread.start()
        self.root.after(100, self._fw_poll)

    def _fw_worker(self, uploader: "FirmwareUploader", path: str, port: str) -> None:
        """Worker thread: flash the board, funnelling all output through the queue."""
        try:
            uploader.upload(
                path, port,
                log_cb=lambda line: self._fw_queue.put(("log", line)),
                progress_cb=lambda pct: self._fw_queue.put(("progress", pct)),
            )
            self._fw_queue.put(("done", True, ""))
        except FirmwareError as exc:
            self._fw_queue.put(("done", False, str(exc)))
        except Exception as exc:                       # surfaced in the UI thread
            self._fw_queue.put(("done", False, f"Unexpected error: {exc}"))

    def _fw_poll(self) -> None:
        """Drain worker messages into the UI; re-arms itself until the thread exits."""
        finished = False
        while True:
            try:
                msg = self._fw_queue.get_nowait()
            except queue.Empty:
                break
            kind = msg[0]
            if kind == "log":
                self._fw_log(msg[1])
            elif kind == "progress":
                self._fw_pb["value"] = msg[1]
            elif kind == "done":
                finished = True
                self._fw_finish(bool(msg[1]), str(msg[2]))
        if not finished:
            self.root.after(100, self._fw_poll)

    def _fw_finish(self, ok: bool, error: str) -> None:
        self._fw_upload_btn.state(["!disabled"])
        self._connect_btn.state(["!disabled"])
        if ok:
            self._fw_pb["value"] = 100
            self._set_status("Firmware update complete.")
        else:
            self._fw_pb["value"] = 0
            self._fw_log(f"UPDATE FAILED: {error}")
            self._set_status("Firmware update failed.")
            messagebox.showerror("Firmware update failed", error)

        if not self._fw_was_connected:
            if ok:
                self._fw_log("Press Connect to verify the new firmware.")
            return
        self._fw_log(f"Waiting {BOOTLOADER_EXIT_MS / 1000:.0f}s for the gauge to restart...")
        self.root.after(BOOTLOADER_EXIT_MS, self._fw_reconnect)

    def _fw_reconnect(self) -> None:
        """Re-open the port after flashing and confirm the new firmware responds."""
        self._fw_was_connected = False
        if not self._serial:
            return
        selected_port = self._port_var.get()
        port = self._port_devices.get(selected_port, selected_port)
        try:
            baud = int(self._baud_var.get())
        except ValueError:
            self._fw_log("Reconnect failed: invalid baud rate.")
            return

        def done(error: Optional[Exception], version: str) -> None:
            if isinstance(error, ConnectCancelled):
                self._fw_log("Reconnect cancelled.")
                self._conn_status_var.set("Disconnected")
            elif error is not None:
                self._fw_log(f"Reconnect failed: {error}")
                self._conn_status_var.set("Disconnected")
            elif not version:
                self._fw_log("The gauge did not respond after the update. "
                             "Unplug it, plug it back in, then press Connect.")
            else:
                self._fw_var.set(version)
                self._conn_status_var.set("Connected")
                self._connect_btn.configure(text="Disconnect")
                self._fw_log(f"Reconnected — {version}")
                self._set_status(f"Connected: {port}  |  {version}")
                self._read_from_device()

        self._connect_with_progress(port, baud, done)

    def _refresh_ports(self) -> None:
        details = (
            self._serial.list_port_details()
            if HAS_SERIAL and self._serial else []
        )
        self._port_devices = {display: device for device, display in details}
        displays = list(self._port_devices)
        self._port_combo["values"] = displays
        if displays and self._port_var.get() not in self._port_devices:
            self._port_var.set(displays[0])
        elif not displays:
            self._port_var.set("")

    def _toggle_connection(self) -> None:
        if not HAS_SERIAL or self._serial is None:
            messagebox.showerror("Error", "pyserial is not installed.\nRun: pip install pyserial")
            return
        if self._serial.is_connected:
            self._test_restore_sources()
            self._serial.disconnect()
            self._conn_status_var.set("Disconnected")
            self._connect_btn.configure(text="Connect")
            self._set_status("Disconnected")
        else:
            selected_port = self._port_var.get()
            port = self._port_devices.get(selected_port, selected_port)
            try:
                baud = int(self._baud_var.get())
            except ValueError:
                messagebox.showerror("Error", "Invalid baud rate.")
                return

            def done(error: Optional[Exception], ver: str) -> None:
                if isinstance(error, ConnectCancelled):
                    self._set_status("Connection cancelled")
                elif error is not None:
                    messagebox.showerror("Connection error", str(error))
                elif not ver:
                    messagebox.showwarning("Warning", "Connected but Arduino did not respond to ping.")
                else:
                    self._fw_var.set(ver)
                    self._conn_status_var.set("Connected")
                    self._connect_btn.configure(text="Disconnect")
                    self._set_status(f"Connected: {port} @ {baud}  |  {ver}")

            self._connect_with_progress(port, baud, done)

    def _connect_with_progress(self, port: str, baud: int,
                               on_done: Callable[[Optional[Exception], str], None]) -> None:
        """Connect on a worker thread behind a modal, cancellable progress dialog.

        *on_done(error, version)* runs on the UI thread; *version* is empty when
        the gauge connected but did not answer the ping.
        """
        assert self._serial is not None
        serial_obj = self._serial
        results: "queue.Queue[tuple]" = queue.Queue()
        cancel = threading.Event()

        def worker() -> None:
            try:
                serial_obj.connect(port, baud,
                                   progress=lambda m: results.put(("msg", m)),
                                   cancel=cancel)
                if serial_obj.ping():
                    results.put(("done", None, serial_obj.version()))
                else:
                    serial_obj.disconnect()
                    results.put(("done", None, ""))
            except Exception as exc:
                serial_obj.disconnect()
                results.put(("done", exc, ""))

        dlg = tk.Toplevel(self.root)
        dlg.title("Connecting")
        dlg.resizable(False, False)
        dlg.transient(self.root)
        body = ttk.Frame(dlg, padding=16)
        body.pack(fill="both", expand=True)
        ttk.Label(body, text=f"Connecting to {port}", font=("TkDefaultFont", 10, "bold")).pack(anchor="w")
        msg_var = tk.StringVar(value="Starting...")
        ttk.Label(body, textvariable=msg_var, width=44).pack(anchor="w", pady=(6, 10))
        bar = ttk.Progressbar(body, mode="indeterminate", length=320)
        bar.pack(fill="x")
        bar.start(12)

        def request_cancel() -> None:
            cancel.set()
            msg_var.set("Cancelling...")
            cancel_btn.state(["disabled"])

        cancel_btn = ttk.Button(body, text="Cancel", command=request_cancel)
        cancel_btn.pack(anchor="e", pady=(12, 0))
        dlg.protocol("WM_DELETE_WINDOW", request_cancel)
        dlg.update_idletasks()
        x = self.root.winfo_rootx() + (self.root.winfo_width() - dlg.winfo_width()) // 2
        y = self.root.winfo_rooty() + (self.root.winfo_height() - dlg.winfo_height()) // 2
        dlg.geometry(f"+{max(x, 0)}+{max(y, 0)}")
        dlg.grab_set()
        self._connect_btn.state(["disabled"])

        def poll() -> None:
            finished = None
            while True:
                try:
                    item = results.get_nowait()
                except queue.Empty:
                    break
                if item[0] == "msg":
                    if not cancel.is_set():
                        msg_var.set(item[1])
                else:
                    finished = item
            if finished is None:
                self.root.after(100, poll)
                return
            bar.stop()
            dlg.grab_release()
            dlg.destroy()
            self._connect_btn.state(["!disabled"])
            on_done(finished[1], finished[2])

        threading.Thread(target=worker, daemon=True).start()
        self.root.after(100, poll)

    # ------ Tab 2: Motors ------
    def _build_tab_motors(self, frame: ttk.Frame) -> None:
        canvas = tk.Canvas(frame)
        scroll = ttk.Scrollbar(frame, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)
        inner = ttk.Frame(canvas)
        canvas.create_window((0, 0), window=inner, anchor="nw")
        inner.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        for idx, (title, sweep_param, (func_p, min_p, max_p)) in enumerate(zip(
            ("Motor 1", "Motor 2", "Motor 3", "Motor 4"),
            ("m1_sweep", "m2_sweep", "m3_sweep", "m4_sweep"),
            MOTOR_PARAMS,
        )):
            grp = ttk.LabelFrame(inner, text=title)
            grp.grid(row=idx, column=0, padx=10, pady=6, sticky="new")
            func_combo = self._param_row(grp, 0, 0, "Function:", func_p)
            self._motor_angle_row(grp, 1, sweep_param)
            self._gauge_range_rows(grp, 2, func_p, min_p, max_p)
            func_combo.bind(
                "<<ComboboxSelected>>",
                lambda _e, f=func_p: self._apply_function_defaults(f),
                add="+",
            )

        ms_sweep_grp = ttk.LabelFrame(inner, text="Motor S (Speedometer)")
        ms_sweep_grp.grid(row=0, column=1, padx=10, pady=6, sticky="new")
        self._param_row(ms_sweep_grp, 0, 0, "Sweep steps:", "ms_sweep")
        ms_grp = ttk.LabelFrame(inner, text="Motor S - Zeroing")
        ms_grp.grid(row=1, column=1, padx=10, pady=6, sticky="new")
        self._param_row(ms_grp, 0, 0, "Zero step delay (us):", "ms_zero_delay")
        self._param_row(ms_grp, 1, 0, "Zero sweep fraction:",  "ms_zero_factor")
        sw_grp = ttk.LabelFrame(inner, text="Startup sweep")
        sw_grp.grid(row=2, column=1, padx=10, pady=6, sticky="new")
        self._param_row(sw_grp, 0, 0, "Sweep time (ms):", "motor_sweep_ms")

    def _motor_angle_row(self, parent: tk.Widget, row: int, param: str) -> None:
        """Sweep entry in degrees; the config value stays in motor steps."""
        label = ttk.Label(parent, text="Sweep angle (\u00b0):")
        label.grid(row=row, column=0, sticky="e", padx=4, pady=2)
        display_var = tk.StringVar()
        entry = ttk.Entry(
            parent, textvariable=display_var, width=12, validate="key",
            validatecommand=(self.root.register(self._angle_key_ok), "%P"),
        )
        entry.grid(row=row, column=1, sticky="w", padx=4, pady=2)
        self._angle_vars[param] = display_var
        self._param_labels[param] = label
        self._param_widgets[param] = entry
        tip = (f"Full needle sweep in degrees (Switec Z27.168: {MOTOR_STEPS_PER_DEGREE} steps per degree). "
               "Stored in the config as steps.")
        ToolTip(label, tip)
        ToolTip(entry, tip)
        display_var.trace_add("write", lambda *_a: self._on_angle_changed(param))
        self._vars[param].trace_add("write", lambda *_a: self._sync_angle_display(param))
        self._sync_angle_display(param)

    @staticmethod
    def _angle_key_ok(proposed: str) -> bool:
        """Allow only numeric text up to the motor's mechanical limit while typing."""
        if proposed in ("", "."):
            return True
        try:
            value = float(proposed)
        except ValueError:
            return False
        return 0 <= value <= MOTOR_MAX_ANGLE

    def _sync_angle_display(self, param: str) -> None:
        display_var = self._angle_vars[param]
        raw = self._vars[param].get()
        try:
            steps = int(raw)
        except ValueError:
            if not raw:
                display_var.set("")
            return
        try:
            if _angle_to_steps(display_var.get()) == steps:
                return  # already consistent; don't reformat what the user is typing
        except ValueError:
            pass
        self._syncing_angle = True
        try:
            display_var.set(_display_number(steps / MOTOR_STEPS_PER_DEGREE))
        finally:
            self._syncing_angle = False

    def _on_angle_changed(self, param: str) -> None:
        if self._syncing_angle:
            return
        self._mark_parameter_dirty(param)
        text = self._angle_vars[param].get()
        self._syncing_angle = True
        try:
            if not text.strip():
                self._vars[param].set("")
                return
            try:
                steps = _angle_to_steps(text)
            except ValueError:
                return  # reported by _validate_motor_angles when writing
            if self._vars[param].get() != str(steps):
                self._vars[param].set(str(steps))
        finally:
            self._syncing_angle = False

    def _validate_motor_angles(self) -> bool:
        for param, display_var in self._angle_vars.items():
            text = display_var.get().strip()
            if not text:
                continue
            try:
                _angle_to_steps(text)
            except ValueError as exc:
                messagebox.showerror("Invalid sweep angle", f"{param}: {exc}")
                return False
        return True

    def _gauge_range_rows(
        self, parent: tk.Widget, row: int, func_param: str, min_param: str, max_param: str,
    ) -> None:
        """Gauge min/max entries plus CW/CCW radios.

        The entries hold the user's low/high values; the config stores them in the
        min/max parameters, swapped when CCW is selected.
        """
        low_var, high_var = tk.StringVar(), tk.StringVar()
        dir_var = tk.StringVar(value=DIRECTION_OPTIONS[0])
        busy = [False]
        entries = []
        labels = []
        for offset, (text, var) in enumerate((("Gauge min:", low_var), ("Gauge max:", high_var))):
            lbl = ttk.Label(parent, text=text)
            lbl.grid(row=row + offset, column=0, sticky="e", padx=4, pady=2)
            entry = ttk.Entry(parent, textvariable=var, width=12)
            entry.grid(row=row + offset, column=1, sticky="w", padx=4, pady=2)
            labels.append(lbl)
            entries.append(entry)
        dir_lbl = ttk.Label(parent, text="Sweep direction:")
        dir_lbl.grid(row=row + 2, column=0, sticky="e", padx=4, pady=2)
        radios = ttk.Frame(parent)
        radios.grid(row=row + 2, column=1, sticky="w", padx=4, pady=2)
        for text in DIRECTION_OPTIONS:
            radio = ttk.Radiobutton(radios, text=text, variable=dir_var, value=text)
            radio.pack(side="left", padx=(0, 8))
            ToolTip(radio, TOOLTIPS["motor_direction"])
        ToolTip(dir_lbl, TOOLTIPS["motor_direction"])
        for param, lbl, entry in zip((min_param, max_param), labels, entries):
            self._param_labels[param] = lbl
            self._param_widgets[param] = entry
            ToolTip(lbl, TOOLTIPS[param])
            ToolTip(entry, TOOLTIPS[param])
        self._gauge_display_vars[func_param] = (low_var, high_var)

        def to_device(*_args: object) -> None:
            if busy[0]:
                return
            busy[0] = True
            try:
                dev_min, dev_max = _gauge_range_to_device(
                    low_var.get(), high_var.get(), dir_var.get() == "CCW",
                )
                self._vars[min_param].set(dev_min)
                self._vars[max_param].set(dev_max)
                self._mark_parameter_dirty(min_param)
                self._mark_parameter_dirty(max_param)
            finally:
                busy[0] = False

        def from_device(*_args: object) -> None:
            if busy[0]:
                return
            busy[0] = True
            try:
                low, high, ccw = _gauge_range_from_device(
                    self._vars[min_param].get(), self._vars[max_param].get(),
                )
                low_var.set(low)
                high_var.set(high)
                dir_var.set("CCW" if ccw else DIRECTION_OPTIONS[0])
            finally:
                busy[0] = False

        for var in (low_var, high_var, dir_var):
            var.trace_add("write", to_device)
        self._vars[min_param].trace_add("write", from_device)
        self._vars[max_param].trace_add("write", from_device)
        self._vars[func_param].trace_add(
            "write",
            lambda *_a: self._update_gauge_range_labels(func_param, min_param, max_param),
        )
        self._update_gauge_range_labels(func_param, min_param, max_param)

    def _update_gauge_range_labels(self, func_param: str, min_param: str, max_param: str) -> None:
        try:
            unit = MOTOR_FUNCTION_UNITS[int(self._vars[func_param].get())]
        except (ValueError, IndexError):
            unit = ""
        suffix = f" ({unit})" if unit else ""
        self._param_labels[min_param].configure(text=f"Gauge min{suffix}:")
        self._param_labels[max_param].configure(text=f"Gauge max{suffix}:")

    def _apply_function_defaults(self, func_param: str) -> None:
        """Seed the gauge min/max with typical values after the user picks a function."""
        try:
            lo, hi = MOTOR_FUNCTION_RANGES[int(self._vars[func_param].get())]
        except (ValueError, IndexError):
            return
        if (lo, hi) != (0, 0):
            low_var, high_var = self._gauge_display_vars[func_param]
            low_var.set(str(lo))
            high_var.set(str(hi))

    def _validate_motor_ranges(self) -> bool:
        for title, (func_p, min_p, max_p) in zip(
            ("Motor 1", "Motor 2", "Motor 3", "Motor 4"), MOTOR_PARAMS,
        ):
            if self._vars[func_p].get() in ("", "0"):
                continue
            try:
                lo = int(self._vars[min_p].get())
                hi = int(self._vars[max_p].get())
            except ValueError:
                messagebox.showerror("Invalid gauge range", f"{title}: gauge min and max must be whole numbers.")
                return False
            if not (-50 <= lo <= 1000 and -50 <= hi <= 1000) or lo == hi:
                messagebox.showerror(
                    "Invalid gauge range",
                    f"{title}: gauge min and max must differ (both between -50 and 1000).",
                )
                return False
        return True

    # ------ Signal source tab ------
    def _build_tab_signal_source(self, frame: ttk.Frame) -> None:
        canvas = tk.Canvas(frame, highlightthickness=0)
        scrollbar = ttk.Scrollbar(frame, orient="vertical", command=canvas.yview)
        content = ttk.Frame(canvas)
        window = canvas.create_window((0, 0), window=content, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        content.bind(
            "<Configure>",
            lambda _event: canvas.configure(scrollregion=canvas.bbox("all")),
        )
        canvas.bind(
            "<Configure>",
            lambda event: canvas.itemconfigure(window, width=event.width),
        )
        scrollbar.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        protocol = ttk.LabelFrame(content, text="CAN Bus")
        protocol.pack(padx=10, pady=6, fill="x")
        self._param_row(protocol, 0, 0, "CAN protocol:", "can_protocol")

        speed_source = ttk.LabelFrame(content, text="Speed Source")
        speed_source.pack(padx=10, pady=6, fill="x")
        self._param_row(speed_source, 0, 0, "Speed source:", "speed_source")

        rpm_source = ttk.LabelFrame(content, text="RPM Source")
        rpm_source.pack(padx=10, pady=6, fill="x")
        self._param_row(rpm_source, 0, 0, "RPM source:", "rpm_source")

        other_sources = ttk.LabelFrame(content, text="Other Signal Sources")
        other_sources.pack(padx=10, pady=6, fill="x")
        for i, (label, param) in enumerate([
            ("Oil pressure:", "oil_prs_source"),
            ("Fuel pressure:", "fuel_prs_source"),
            ("Coolant temp:", "coolant_src"),
            ("Oil temp:", "oil_temp_src"),
            ("MAP / boost:", "map_source"),
            ("Lambda / AFR:", "lambda_source"),
            ("Fuel level:", "fuel_lvl_src"),
        ]):
            self._param_row(other_sources, i, 0, label, param)

    def _build_tab_speed_rpm(self, frame: ttk.Frame) -> None:
        self._build_tab_speed(frame)
        self._build_tab_rpm(frame)
        odo = ttk.LabelFrame(frame, text="Mechanical odometer calibration")
        odo.pack(padx=10, pady=6, fill="both")
        self._param_row(odo, 0, 0, "Steps per revolution:", "odo_steps")
        self._param_row(odo, 1, 0, "Motor gear teeth:",     "odo_motor_teeth")
        self._param_row(odo, 2, 0, "Driven gear teeth:",    "odo_gear_teeth")

    # ------ Speed source configuration ------
    def _build_tab_speed(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Speed & Hall Sensor")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "Revs per km:",    "revs_per_km")
        self._param_row(grp, 1, 0, "Teeth per rev:",  "teeth_per_rev")
        hall_label = ttk.Label(grp, text="Minimum Speed Cutoff (km/h):")
        hall_label.grid(row=2, column=0, sticky="e", padx=4, pady=2)
        self._param_labels["hall_speed_min"] = hall_label
        hall_entry = ttk.Entry(grp, textvariable=self._hall_speed_min_display_var, width=12)
        hall_entry.grid(row=2, column=1, sticky="w", padx=4, pady=2)
        self._param_widgets["hall_speed_min"] = hall_entry
        hall_entry.bind(
            "<FocusOut>",
            lambda _event: self._commit_speed_display("hall_speed_min"),
        )
        self._speed_display_widgets["hall_speed_min"] = hall_entry
        ToolTip(hall_label, TOOLTIPS["hall_speed_min"])
        ToolTip(hall_entry, TOOLTIPS["hall_speed_min"])
        self._hall_speed_min_display_var.trace_add(
            "write", lambda *_args: self._on_speed_display_changed("hall_speed_min"),
        )
        self._vars["hall_speed_min"].trace_add(
            "write", lambda *_args: self._sync_speed_display("hall_speed_min"),
        )
        self._param_row(grp, 3, 0, "Hall filter:",    "filter_hall")
        speedo_label = ttk.Label(grp, text="Speedometer Max:")
        speedo_label.grid(row=4, column=0, sticky="e", padx=4, pady=2)
        self._param_labels["speedo_max"] = speedo_label
        speedo_entry = ttk.Entry(grp, textvariable=self._speedo_max_display_var, width=12)
        speedo_entry.grid(row=4, column=1, sticky="w", padx=4, pady=2)
        self._param_widgets["speedo_max"] = speedo_entry
        speedo_entry.bind(
            "<FocusOut>",
            lambda _event: self._commit_speed_display("speedo_max"),
        )
        self._speed_display_widgets["speedo_max"] = speedo_entry
        ToolTip(speedo_label, TOOLTIPS["speedo_max"])
        ToolTip(speedo_entry, TOOLTIPS["speedo_max"])
        self._speedo_max_display_var.trace_add(
            "write", lambda *_args: self._on_speed_display_changed("speedo_max"),
        )
        self._vars["speedo_max"].trace_add(
            "write", lambda *_args: self._sync_speed_display("speedo_max"),
        )
        speed_units = self._unit_radio_group(
            grp, 4, 2, self._speedo_max_units_var, SPEEDO_MAX_UNITS,
            self._speedo_max_units_changed,
        )
        self._source_dependent_widgets["speedo_max"] = speed_units
        self._sync_speed_display("hall_speed_min")
        self._sync_speed_display("speedo_max")

    # ------ RPM source configuration ------
    def _build_tab_rpm(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Engine RPM Sensor")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "Cylinder count:",  "cyl_count")
        self._param_row(grp, 1, 0, "Debounce (us):",   "rpm_debounce_us")
        self._param_row(grp, 2, 0, "Engine RPM min:",  "engine_rpm_min")
        self._param_row(grp, 3, 0, "RPM filter:",      "filter_rpm")

    # ------ Tab 5: Sensors ------
    def _build_tab_sensors(self, frame: ttk.Frame) -> None:
        for col in range(2):
            frame.columnconfigure(col, weight=1)

        therm = ttk.LabelFrame(frame, text="Thermistor")
        therm.grid(row=0, column=0, padx=10, pady=10, sticky="nw")
        self._param_row(therm, 0, 0, "Therm filter:", "filter_therm")

        fuel = ttk.LabelFrame(frame, text="Fuel Level")
        fuel.grid(row=0, column=1, padx=10, pady=10, sticky="nw")
        self._param_row(fuel, 0, 0, "Fuel filter:", "filter_fuel")

        av = ttk.LabelFrame(frame, text="Analog Sensor Filters")
        av.grid(row=1, column=0, padx=10, pady=10, sticky="nw")
        self._param_row(av, 0, 0, "AV1 filter:", "filter_av1")
        self._param_row(av, 1, 0, "AV2 filter:", "filter_av2")
        self._param_row(av, 2, 0, "AV3 filter:", "filter_av3")

        batt = ttk.LabelFrame(frame, text="Battery Voltage Sensor")
        batt.grid(row=1, column=1, padx=10, pady=10, sticky="nw")
        self._param_row(batt, 0, 0, "Vbatt filter:", "filter_vbatt")
        self._param_row(batt, 1, 0, "Vbatt scaler:", "vbatt_scaler")

    # ------ Tab 7: LED Tach ------
    def _build_tab_led_tach(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="LED Tachometer")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "LED count:",       "num_leds")
        self._param_row(grp, 1, 0, "Shift RPM:",       "tach_max")
        self._param_row(grp, 2, 0, "Min display RPM:", "tach_min")
        self._param_row(grp, 3, 0, "Warning LEDs:",    "warn_leds")
        self._param_row(grp, 4, 0, "Shift LEDs:",      "shift_leds")

    # ------ Tab 8: Display ------
    def _build_tab_display(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Units")
        grp.pack(padx=10, pady=10, fill="x")
        self._param_row(grp, 0, 0, "Unit system:", "units")
        screens_frame = ttk.Frame(frame)
        screens_frame.pack(padx=10, pady=6, fill="x")
        for col, (title, param) in enumerate((
            ("Display 1 screens", "disp1_order"),
            ("Display 2 screens", "disp2_order"),
        )):
            box = ttk.LabelFrame(screens_frame, text=title)
            box.grid(row=0, column=col, padx=(0, 8), sticky="n")
            editor = ScreenListEditor(box, self._vars[param])
            editor.pack(padx=6, pady=4)
            ToolTip(box, TOOLTIPS[param])
            self._vars[param].trace_add(
                "write", lambda *_a, p=param: self._mark_parameter_dirty(p),
            )
        ttk.Label(
            screens_frame, justify="left",
            text=("Checked screens are reachable with the rotary encoder.\n"
                  "Drag a screen up or down to reorder.\n"
                  "Display 1 always ends with Settings; display 2 is chosen\n"
                  "from the Settings menu."),
        ).grid(row=0, column=2, sticky="nw")
        self._build_tab_clock(frame)

    # ------ Tab 9: Faults ------
    def _build_tab_faults(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Fault Warning Thresholds")
        grp.pack(padx=10, pady=10, fill="both")
        self._warning_threshold_row(
            grp, 0, "Oil pressure warning:", "oil_warn_kpa",
            OIL_WARNING_UNITS, self._oil_warn_display_var, self._oil_warn_units_var,
        )
        self._warning_threshold_row(
            grp, 1, "Engine temp warning:", "coolant_warn_c",
            TEMPERATURE_UNITS, self._coolant_warn_display_var, self._coolant_warn_units_var,
        )
        self._param_row(grp, 2, 0, "Battery voltage (V):", "batt_warn_v")
        self._param_row(grp, 3, 0, "Engine running RPM:",  "engine_run_rpm")
        self._param_row(grp, 4, 0, "Low fuel (%):",        "fuel_warn_pct")

    # ------ Display Settings: Clock & Fuel group ------
    def _build_tab_clock(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Clock & Fuel")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "UTC offset (hours):",       "clock_offset")
        fuel_label = ttk.Label(grp, text="Fuel tank capacity:")
        fuel_label.grid(row=1, column=0, sticky="e", padx=4, pady=2)
        fuel_entry = ttk.Entry(grp, textvariable=self._fuel_capacity_display_var, width=12)
        fuel_entry.grid(row=1, column=1, sticky="w", padx=4, pady=2)
        self._param_labels["fuel_capacity"] = fuel_label
        self._param_widgets["fuel_capacity"] = fuel_entry
        ToolTip(fuel_label, TOOLTIPS["fuel_capacity"])
        ToolTip(fuel_entry, TOOLTIPS["fuel_capacity"])
        self._fuel_capacity_display_var.trace_add(
            "write", lambda *_args: self._on_fuel_capacity_display_changed(),
        )
        self._vars["fuel_capacity"].trace_add(
            "write", lambda *_args: self._sync_fuel_capacity_display(),
        )
        self._vars["fuel_capacity"].trace_add(
            "write", lambda *_args: self._mark_parameter_dirty("fuel_capacity"),
        )
        self._unit_radio_group(
            grp, 1, 2, self._fuel_capacity_units_var, FUEL_CAPACITY_UNITS,
            self._fuel_capacity_units_changed,
        )
        self._sync_fuel_capacity_display()

    # ------ Test tab ------
    # key -> (label, metric unit, imperial unit, metric range, imperial range,
    #         default metric value, imperial->metric conversion, send method, source param)
    _TEST_SIGNALS = (
        ("speed",   "Speed",        "km/h", "mph", (0, 300),   (0, 186),  0,  lambda v: v * 1.609344,
         "test_speed", "speed_source"),
        ("rpm",     "RPM",          "rpm",  "rpm", (0, 10000), (0, 10000), 0, lambda v: v,
         "test_rpm", "rpm_source"),
        ("oil",     "Oil pressure", "kPa",  "psi", (0, 1000),  (0, 145),  0,  lambda v: v * 6.894757,
         "test_oil_pressure", "oil_prs_source"),
        ("coolant", "Coolant temp", "\u00b0C", "\u00b0F", (-40, 150), (-40, 300), 20,
         lambda v: (v - 32) / 1.8, "test_coolant_temp", "coolant_src"),
        ("fuel",    "Fuel level",   "%",    "%",   (0, 100),  (0, 100),  50, lambda v: v,
         "test_fuel_level", "fuel_lvl_src"),
    )

    def _build_tab_test(self, frame: ttk.Frame) -> None:
        self._test_orig_sources: Dict[str, str] = {}
        self._test_odo_after: Optional[str] = None
        note = ttk.Label(
            frame, wraplength=640, justify="left",
            text="Inject signal values into the gauge. Sending a value temporarily switches "
                 "that signal's source to Serial in the gauge's RAM. The original source is "
                 "restored automatically when you leave this tab, disconnect, read, write or "
                 "close the tool, and is never saved to EEPROM from here.",
        )
        note.pack(padx=10, pady=(10, 4), anchor="w")

        units_row = ttk.Frame(frame)
        units_row.pack(padx=10, pady=2, anchor="w")
        ttk.Label(units_row, text="Input units:").pack(side="left", padx=(0, 6))
        self._test_units_var = tk.StringVar(value="metric")
        self._test_prev_units = "metric"
        for text, value in (("Metric", "metric"), ("'Merican", "imperial")):
            ttk.Radiobutton(
                units_row, text=text, value=value, variable=self._test_units_var,
                command=self._test_units_changed,
            ).pack(side="left", padx=4)
        ttk.Button(
            units_row, text="Restore sources now", command=self._test_restore_sources,
        ).pack(side="left", padx=(20, 0))

        sig = ttk.LabelFrame(frame, text="Signal Injection")
        sig.pack(padx=10, pady=6, fill="x")
        self._test_vars: Dict[str, tk.StringVar] = {}
        self._test_scales: Dict[str, ttk.Scale] = {}
        self._test_labels: Dict[str, ttk.Label] = {}
        for row, (key, name, _mu, _iu, (lo, hi), _ir, default, _conv, method, _src) in enumerate(
            self._TEST_SIGNALS
        ):
            var = tk.StringVar(value=str(default))
            self._test_vars[key] = var
            label = ttk.Label(sig)
            label.grid(row=row, column=0, sticky="e", padx=4, pady=3)
            self._test_labels[key] = label
            scale = ttk.Scale(
                sig, from_=lo, to=hi, length=260,
                command=lambda val, v=var: v.set(str(round(float(val)))),
            )
            scale.set(default)
            scale.grid(row=row, column=1, padx=4, pady=3)
            self._test_scales[key] = scale
            entry = ttk.Entry(sig, textvariable=var, width=8)
            entry.grid(row=row, column=2, padx=4, pady=3)
            send = lambda _e=None, k=key: self._test_send_signal(k)
            ttk.Button(sig, text="Send", command=send).grid(row=row, column=3, padx=4, pady=3)
            entry.bind("<Return>", send)
        self._test_refresh_unit_labels()

        misc = ttk.LabelFrame(frame, text="Other Tests")
        misc.pack(padx=10, pady=6, fill="x")
        ttk.Button(misc, text="Ping", command=self._test_ping).grid(row=0, column=0, padx=4, pady=4)
        ttk.Button(misc, text="Version", command=self._test_version).grid(row=0, column=1, padx=4, pady=4)
        ttk.Label(misc, text="Odometer motor revs (-100 to 100):").grid(row=0, column=2, padx=(16, 4))
        self._test_odo_var = tk.StringVar(value="1")
        ttk.Spinbox(misc, from_=-100, to=100, textvariable=self._test_odo_var, width=6).grid(row=0, column=3)
        self._test_odo_btn = ttk.Button(misc, text="Move", command=self._test_odo_toggle)
        self._test_odo_btn.grid(row=0, column=4, padx=4)
        ttk.Label(misc, text="Splash preview:").grid(row=1, column=0, columnspan=2, padx=4, pady=4, sticky="e")
        ttk.Button(misc, text="Slot 1", command=lambda: self._test_splash(1)).grid(row=1, column=2, padx=4)
        ttk.Button(misc, text="Slot 2", command=lambda: self._test_splash(2)).grid(row=1, column=3, padx=4)

        log_frame = ttk.LabelFrame(frame, text="Log")
        log_frame.pack(padx=10, pady=6, fill="both", expand=True)
        self._test_log = scrolledtext.ScrolledText(log_frame, height=8, state="disabled")
        self._test_log.pack(fill="both", expand=True, padx=4, pady=4)

    def _test_log_line(self, msg: str) -> None:
        self._test_log.configure(state="normal")
        self._test_log.insert("end", msg + "\n")
        self._test_log.see("end")
        self._test_log.configure(state="disabled")

    def _test_run(self, desc: str, action: Callable[[], object]) -> bool:
        """Run a serial action, log the outcome, and report whether it succeeded."""
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return False
        try:
            result = action()
        except Exception as exc:
            self._test_log_line(f"{desc} -> ERROR: {exc}")
            self._set_status(f"Test failed: {exc}")
            return False
        self._test_log_line(f"{desc} -> {result if result else 'ok'}")
        return True

    def _test_signal_spec(self, key: str) -> tuple:
        return next(s for s in self._TEST_SIGNALS if s[0] == key)

    def _test_refresh_unit_labels(self) -> None:
        imperial = self._test_units_var.get() == "imperial"
        for key, name, m_unit, i_unit, m_rng, i_rng, *_rest in self._TEST_SIGNALS:
            lo, hi = i_rng if imperial else m_rng
            self._test_labels[key].configure(text=f"{name} ({i_unit if imperial else m_unit}):")
            self._test_scales[key].configure(from_=lo, to=hi)

    def _test_units_changed(self) -> None:
        new = self._test_units_var.get()
        old = self._test_prev_units
        if new == old:
            return
        for key, _n, _mu, _iu, m_rng, i_rng, _d, conv, *_rest in self._TEST_SIGNALS:
            var = self._test_vars[key]
            try:
                value = float(var.get())
            except ValueError:
                value = None
            if value is not None:
                offset = conv(0)
                slope = conv(1) - offset
                if old == "imperial":
                    value = conv(value)          # imperial -> metric
                if new == "imperial":
                    value = (value - offset) / slope  # metric -> imperial
                lo, hi = i_rng if new == "imperial" else m_rng
                value = min(max(value, lo), hi)
            self._test_scales[key].configure(
                from_=(i_rng if new == "imperial" else m_rng)[0],
                to=(i_rng if new == "imperial" else m_rng)[1],
            )
            if value is not None:
                self._test_scales[key].set(value)
                var.set(str(round(value)))
        self._test_prev_units = new
        self._test_refresh_unit_labels()

    def _test_send_signal(self, key: str) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        var = self._test_vars[key]
        _k, name, _mu, _iu, m_rng, i_rng, _d, conv, method, source = self._test_signal_spec(key)
        imperial = self._test_units_var.get() == "imperial"
        try:
            value = float(var.get())
        except ValueError:
            messagebox.showerror("Invalid value", f"'{var.get()}' is not a number.")
            return
        lo, hi = i_rng if imperial else m_rng
        if not lo <= value <= hi:
            messagebox.showerror("Out of range", f"{name} must be between {lo} and {hi}.")
            return
        metric = conv(value) if imperial else value
        if key in ("speed", "rpm"):
            metric = int(round(metric))
        if not self._test_use_serial_source(source):
            return
        if self._test_run(
            f"{key} = {metric:g}", lambda: getattr(self._serial, method)(metric)
        ):
            self._test_scales[key].set(value)

    def _test_use_serial_source(self, param: str) -> bool:
        """Temporarily switch *param* to its Serial source, remembering the original."""
        if param in self._test_orig_sources:
            return True
        assert self._serial is not None
        try:
            original = self._serial.get(param)
        except Exception as exc:
            self._test_log_line(f"get {param} -> ERROR: {exc}")
            return False
        value = SERIAL_SOURCE_VALUES[param]
        if not self._test_run(f"set {param} {value}", lambda: self._serial.set(param, str(value))):  # type: ignore[union-attr]
            return False
        self._test_orig_sources[param] = original
        return True

    def _test_restore_sources(self) -> None:
        """Put every source changed by the Test tab back to its original value."""
        pending, self._test_orig_sources = self._test_orig_sources, {}
        if not pending:
            return
        for param, original in pending.items():
            if self._serial and self._serial.is_connected:
                self._test_run(f"restore {param} {original}",
                               lambda p=param, o=original: self._serial.set(p, o))  # type: ignore[union-attr]

    def _test_ping(self) -> None:
        self._test_run("ping", lambda: "pong" if self._serial.ping() else "no response")  # type: ignore[union-attr]

    def _test_version(self) -> None:
        self._test_run("version", lambda: self._serial.version())  # type: ignore[union-attr]

    def _test_odo_toggle(self) -> None:
        if self._test_odo_btn.cget("text") == "Stop":
            self._test_run("odo stop", lambda: self._serial.test_odo_stop())  # type: ignore[union-attr]
            self._test_stop_odo_ui()
            return
        try:
            revs = int(self._test_odo_var.get())
        except ValueError:
            messagebox.showerror("Invalid value", "Revolutions must be an integer.")
            return
        if not -100 <= revs <= 100:
            messagebox.showerror("Out of range", "Revolutions must be between -100 and 100.")
            return
        if revs == 0:
            return
        if self._test_run(f"odo motor {revs}", lambda: self._serial.test_odo_motor(revs)):  # type: ignore[union-attr]
            self._test_odo_btn.configure(text="Stop")
            # Motor runs at ~4 RPM (15 s per revolution); revert the button when it should be done
            self._test_odo_after = self.root.after(int(abs(revs) * 15000 + 1000), self._test_stop_odo_ui)

    def _test_stop_odo_ui(self) -> None:
        if self._test_odo_after is not None:
            self.root.after_cancel(self._test_odo_after)
            self._test_odo_after = None
        self._test_odo_btn.configure(text="Move")

    def _test_splash(self, slot: int) -> None:
        self._test_run(f"splash test {slot}", lambda: self._serial.splash_test(slot))  # type: ignore[union-attr]

    # ------ Tab 9: Splash ------
    def _build_tab_splash(self, frame: ttk.Frame) -> None:
        for slot in (1, 2):
            grp = ttk.LabelFrame(frame, text=f"Splash Slot {slot}")
            grp.pack(padx=10, pady=10, fill="x")
            canvas = tk.Canvas(grp, width=128 * 3, height=32 * 3, background="black",
                               bd=2, relief="sunken")
            canvas.grid(row=0, column=0, rowspan=4, padx=8, pady=8)
            setattr(self, f"_splash_canvas_{slot}", canvas)
            ttk.Button(grp, text="Load image...",
                       command=lambda s=slot: self._splash_load(s)
                       ).grid(row=0, column=1, padx=4, pady=2, sticky="ew")
            ttk.Button(grp, text="Upload to device",
                       command=lambda s=slot: self._splash_upload(s)
                       ).grid(row=1, column=1, padx=4, pady=2, sticky="ew")
            ttk.Button(grp, text="Test on device",
                       command=lambda s=slot: self._splash_test(s)
                       ).grid(row=2, column=1, padx=4, pady=2, sticky="ew")
            ttk.Button(grp, text="Clear slot",
                       command=lambda s=slot: self._splash_clear(s)
                       ).grid(row=3, column=1, padx=4, pady=2, sticky="ew")
            pb = ttk.Progressbar(grp, orient="horizontal", length=200, mode="determinate")
            pb.grid(row=4, column=0, columnspan=2, padx=8, pady=4, sticky="ew")
            setattr(self, f"_splash_pb_{slot}", pb)

    def _splash_load(self, slot: int) -> None:
        path = filedialog.askopenfilename(
            title=f"Select image for Splash Slot {slot}",
            filetypes=[("Image files", "*.png *.bmp *.jpg *.jpeg *.gif"), ("All", "*.*")],
        )
        if not path:
            return
        if not HAS_PIL:
            messagebox.showerror("Error", "Pillow is required: pip install Pillow")
            return
        try:
            conv = SSD1306Converter()
            data = conv.convert(path)
            self._splash_data[slot] = data
            self._splash_preview(slot, data)
            self._set_status(f"Slot {slot}: loaded {os.path.basename(path)}")
        except Exception as exc:
            messagebox.showerror("Load error", str(exc))

    def _splash_preview(self, slot: int, data: bytes) -> None:
        canvas: tk.Canvas = getattr(self, f"_splash_canvas_{slot}")
        canvas.delete("all")
        if not HAS_PIL:
            return
        conv = SSD1306Converter()
        img = conv.preview(data)
        for y in range(32):
            for x in range(128):
                px = img.getpixel((x, y))
                color = "#ffffff" if px else "#000000"
                canvas.create_rectangle(x*3, y*3, x*3+3, y*3+3, fill=color, outline="")

    def _splash_upload(self, slot: int) -> None:
        if slot not in self._splash_data:
            messagebox.showwarning("No image", f"Load an image for Slot {slot} first.")
            return
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        if not HAS_UPLOADER:
            messagebox.showerror("Error", "pyserial and Pillow are required for upload.")
            return
        pb: ttk.Progressbar = getattr(self, f"_splash_pb_{slot}")
        pb["value"] = 0
        self.root.update_idletasks()
        uploader = SplashUploader(self._serial)
        try:
            def _progress(pct: float) -> None:
                pb["value"] = pct
                self.root.update_idletasks()
            uploader.upload_bytes(slot, self._splash_data[slot], progress_cb=_progress)
            pb["value"] = 100
            self._set_status(f"Slot {slot}: upload complete.")
        except Exception as exc:
            messagebox.showerror("Upload error", str(exc))

    def _splash_test(self, slot: int) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        try:
            self._serial.splash_test(slot)
            self._set_status(f"Slot {slot}: test command sent.")
        except Exception as exc:
            messagebox.showerror("Error", str(exc))

    def _splash_clear(self, slot: int) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        if not messagebox.askyesno("Confirm", f"Clear Slot {slot}? This cannot be undone."):
            return
        try:
            self._serial.splash_clear(slot)
            canvas: tk.Canvas = getattr(self, f"_splash_canvas_{slot}")
            canvas.delete("all")
            self._splash_data.pop(slot, None)
            self._set_status(f"Slot {slot}: cleared.")
        except Exception as exc:
            messagebox.showerror("Error", str(exc))

    def _read_from_device(self) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        self._test_restore_sources()
        try:
            params = self._serial.dump()
            for k, v in params.items():
                if k in self._vars:
                    self._vars[k].set(v)
                    self._dirty[k] = False
            self._enforce_tach_min()
            self._sync_warning_display("oil_warn_kpa")
            self._sync_warning_display("coolant_warn_c")
            self._sync_fuel_capacity_display()
            self._set_status(f"Read {len(params)} parameters from device.")
        except Exception as exc:
            messagebox.showerror("Read error", str(exc))

    def _write_to_device(self) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        self._test_restore_sources()
        self._enforce_tach_min()
        if not self._validate_motor_angles():
            return
        if not self._validate_speed_inputs():
            return
        if not self._validate_warning_inputs():
            return
        if not self._validate_fuel_capacity():
            return
        if not self._validate_motor_ranges():
            return
        selected_sources = self._selected_sources()
        dirty = [
            p for p in ALL_PARAMS
            if self._dirty[p] and self._vars[p].get()
            and _source_uses_parameter(p, selected_sources)
        ]
        if not dirty:
            messagebox.showinfo("Nothing to write", "No parameters have been edited.")
            return
        try:
            for p in dirty:
                self._serial.set(p, self._vars[p].get())
                self._dirty[p] = False
            self._serial.save()
            self._set_status(f"Wrote {len(dirty)} parameter(s) to device and saved.")
        except Exception as exc:
            messagebox.showerror("Write error", str(exc))

    def _reset_defaults(self) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        if not messagebox.askyesno("Confirm", "Reset all parameters to firmware defaults?"):
            return
        self._test_restore_sources()
        try:
            self._serial.reset()
            self._read_from_device()
            self._set_status("Parameters reset to firmware defaults.")
        except Exception as exc:
            messagebox.showerror("Error", str(exc))

    def _save_to_file(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Save configuration",
            defaultextension=".txt",
            filetypes=[("Config files", "*.txt"), ("All files", "*.*")],
            initialfile=ConfigFile.default_filename(),
        )
        if not path:
            return
        self._enforce_tach_min()
        if not self._validate_motor_angles():
            return
        if not self._validate_speed_inputs():
            return
        if not self._validate_warning_inputs():
            return
        if not self._validate_fuel_capacity():
            return
        if not self._validate_motor_ranges():
            return
        params = {p: self._vars[p].get() for p in ALL_PARAMS if self._vars[p].get()}
        params["config_format"] = str(CONFIG_FILE_FORMAT)
        params["speedo_max_units"] = self._speedo_max_units_var.get()
        params["oil_warn_units"] = self._oil_warn_units_var.get()
        params["coolant_warn_units"] = self._coolant_warn_units_var.get()
        params["fuel_capacity_units"] = self._fuel_capacity_units_var.get()
        try:
            self._config_file.save_to_file(path, params)
            self._set_status(f"Configuration saved to {path}")
        except Exception as exc:
            messagebox.showerror("Save error", str(exc))

    def _load_from_file(self) -> None:
        path = filedialog.askopenfilename(
            title="Load configuration",
            filetypes=[("Config files", "*.txt"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            params = self._config_file.load_from_file(path)
            converted_params, units = _decode_config_units(params)
            self._speedo_max_units_var.set(units["speedo_max_units"])
            self._oil_warn_units_var.set(units["oil_warn_units"])
            self._coolant_warn_units_var.set(units["coolant_warn_units"])
            self._fuel_capacity_units_var.set(units["fuel_capacity_units"])
            loaded_count = 0
            for k, v in params.items():
                if k in (
                    "config_format", "speedo_max_units",
                    "oil_warn_units", "coolant_warn_units", "fuel_capacity_units",
                ):
                    continue
                if k in self._vars:
                    self._vars[k].set(converted_params.get(k, v))
                    self._dirty[k] = True
                    loaded_count += 1
            self._enforce_tach_min()
            self._sync_warning_display("oil_warn_kpa")
            self._sync_warning_display("coolant_warn_c")
            self._sync_fuel_capacity_display()
            self._set_status(f"Loaded {loaded_count} parameters from {os.path.basename(path)}")
        except Exception as exc:
            messagebox.showerror("Load error", str(exc))

    def run(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    app = GaugeConfigApp()
    app.run()
