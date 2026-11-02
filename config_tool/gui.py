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
    from serial_protocol import GaugeSerial, SerialError, NotConnectedError
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
    "oil_prs_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic"),
    "fuel_prs_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic"),
    "coolant_src": ("Off", "CAN", "Thermistor", "Synthetic"),
    "oil_temp_src": ("Off", "CAN", "Thermistor"),
    "map_source": ("Off", "CAN", "AV1", "AV2", "AV3", "Synthetic"),
    "lambda_source": ("Off", "CAN", "AV1", "AV2", "AV3"),
    "fuel_lvl_src": ("Off", "Analog sensor", "Synthetic"),
}

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
]

TOOLTIPS: Dict[str, str] = {
    "m1_sweep":          "Motor 1 full sweep in steps (100-4095). 58 x 12 = 696.",
    "m2_sweep":          "Motor 2 full sweep in steps (100-4095).",
    "m3_sweep":          "Motor 3 full sweep in steps (100-4095).",
    "m4_sweep":          "Motor 4 full sweep in steps (100-4095).",
    "ms_sweep":          "Motor S (speedometer NEMA14) sweep steps (100-8000).",
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
    "can_protocol":      "CAN protocol: 0=Haltech v2 1=Megasquirt 2=AiM 3=OBDII",
    "units":             "Unit system: 0=Metric 1=Imperial",
}


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
        self._hall_speed_min_display_var = tk.StringVar(value="")
        self._speedo_max_display_var = tk.StringVar(value="")
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
        ]
        for title, builder in tabs:
            frame = ttk.Frame(nb)
            nb.add(frame, text=title)
            builder(frame)
        self._update_source_dependent_controls()

    def _param_row(self, parent: tk.Widget, row: int, col: int,
                   label: str, param: str, width: int = 12) -> ttk.Entry:
        lbl = ttk.Label(parent, text=label)
        lbl.grid(row=row, column=col, sticky="e", padx=4, pady=2)
        self._param_labels[param] = lbl
        if param in SOURCE_OPTIONS:
            options = SOURCE_OPTIONS[param]
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
        if min_display_rpm < shift_rpm:
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
        self._port_combo = ttk.Combobox(inner, textvariable=self._port_var, width=20)
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
            self._serial.connect(port, int(self._baud_var.get()))
            if not self._serial.ping():
                self._serial.disconnect()
                self._fw_log("The gauge did not respond after the update. "
                             "Unplug it, plug it back in, then press Connect.")
                return
            version = self._serial.version()
            self._fw_var.set(version)
            self._conn_status_var.set("Connected")
            self._connect_btn.configure(text="Disconnect")
            self._fw_log(f"Reconnected — {version}")
            self._set_status(f"Connected: {port}  |  {version}")
            self._read_from_device()
        except Exception as exc:
            self._fw_log(f"Reconnect failed: {exc}")
            self._conn_status_var.set("Disconnected")

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
            self._serial.disconnect()
            self._conn_status_var.set("Disconnected")
            self._connect_btn.configure(text="Connect")
            self._set_status("Disconnected")
        else:
            selected_port = self._port_var.get()
            port = self._port_devices.get(selected_port, selected_port)
            baud = int(self._baud_var.get())
            try:
                self._serial.connect(port, baud)
                if self._serial.ping():
                    ver = self._serial.version()
                    self._fw_var.set(ver)
                    self._conn_status_var.set("Connected")
                    self._connect_btn.configure(text="Disconnect")
                    self._set_status(f"Connected: {port} @ {baud}  |  {ver}")
                else:
                    self._serial.disconnect()
                    messagebox.showwarning("Warning", "Connected but Arduino did not respond to ping.")
            except Exception as exc:
                messagebox.showerror("Connection error", str(exc))

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
        motors = [
            ("Motor 1", "m1_sweep"),
            ("Motor 2", "m2_sweep"),
            ("Motor 3", "m3_sweep"),
            ("Motor 4", "m4_sweep"),
            ("Motor S (Speedometer)", "ms_sweep"),
        ]
        for idx, (title, param) in enumerate(motors):
            grp = ttk.LabelFrame(inner, text=title)
            grp.grid(row=idx, column=0, padx=10, pady=6, sticky="ew")
            self._param_row(grp, 0, 0, "Sweep steps:", param)
        ms_grp = ttk.LabelFrame(inner, text="Motor S - Zeroing")
        ms_grp.grid(row=len(motors), column=0, padx=10, pady=6, sticky="ew")
        self._param_row(ms_grp, 0, 0, "Zero step delay (us):", "ms_zero_delay")
        self._param_row(ms_grp, 1, 0, "Zero sweep fraction:",  "ms_zero_factor")
        sw_grp = ttk.LabelFrame(inner, text="Startup sweep")
        sw_grp.grid(row=len(motors) + 1, column=0, padx=10, pady=6, sticky="ew")
        self._param_row(sw_grp, 0, 0, "Sweep time (ms):", "motor_sweep_ms")

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
        ttk.Label(
            protocol, text="0=Haltech v2  1=Megasquirt  2=AiM  3=OBDII",
            foreground="gray",
        ).grid(row=0, column=2, sticky="w", padx=8)

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
        self._param_row(grp, 0, 0, "Unit system (0=Metric, 1=Imperial):", "units")
        info = ttk.LabelFrame(frame, text="Display 1 screen IDs (reference)")
        info.pack(padx=10, pady=6, fill="both")
        screens = [
            "0: Settings menu", "1: Oil Pressure", "2: Coolant Temp",
            "3: Fuel Level", "4: Battery Voltage", "5: Engine RPM",
            "6: Vehicle Speed", "7: Air/Fuel Ratio", "8: Fuel Pressure",
            "9: Boost (bar)", "10: Boost (text)", "11: Oil Temp",
            "12: Fuel Composition", "13: Injector Duty", "14: Ignition Timing",
            "15: Trip Odometer", "16: Clock", "17: Falcon Script logo",
        ]
        for i, name in enumerate(screens):
            ttk.Label(info, text=name, width=24).grid(row=i, column=0, sticky="w", padx=4, pady=1)
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
        self._enforce_tach_min()
        if not self._validate_speed_inputs():
            return
        if not self._validate_warning_inputs():
            return
        if not self._validate_fuel_capacity():
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
        if not self._validate_speed_inputs():
            return
        if not self._validate_warning_inputs():
            return
        if not self._validate_fuel_capacity():
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
