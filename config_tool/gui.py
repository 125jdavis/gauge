"""
gui.py
======
Tkinter-based GUI for the Gauge V4 Configuration Tool.

Implements CONFIG_TOOL_SPECIFICATION.md §12 (Python Application Specification):
  Tab 1  - Connection
  Tab 2  - Motor Configuration
  Tab 3  - Speed & Hall Sensor
  Tab 4  - Engine RPM
  Tab 5  - Sensors & Curves
  Tab 6  - CAN Bus
  Tab 7  - LED Tachometer
  Tab 8  - Display Settings
  Tab 9  - Fault Warnings
  Tab 10 - Clock & Fuel
  Tab 11 - Splash Screens

Run with: python gui.py
"""

from __future__ import annotations

import os
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from typing import Dict, Optional

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

from config_file import ConfigFile

TOOL_VERSION = "1.0"
WINDOW_TITLE = f"Gauge V4 Configuration Tool v{TOOL_VERSION}"

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
    "hall_speed_min":    "Minimum reportable speed in km/h x 100 (0-100).",
    "cyl_count":         "Engine cylinder count (2-16).",
    "filter_rpm":        "RPM EMA filter coefficient (1-255).",
    "rpm_debounce_us":   "RPM pulse debounce window us (100-20000).",
    "engine_rpm_min":    "Minimum displayed RPM (0-255).",
    "speedo_max":        "Max speedometer reading mph x 100 (1000-30000).",
    "num_leds":          "Total LED strip count (1-64).",
    "warn_leds":         "Warning zone LEDs per side (0-32).",
    "shift_leds":        "Shift-light LEDs per side (0-16).",
    "tach_max":          "Shift RPM (1000-15000).",
    "tach_min":          "Minimum lit RPM (0-5000).",
    "odo_steps":         "Odometer motor steps per revolution (512-8192).",
    "odo_motor_teeth":   "Odometer motor gear tooth count (1-64).",
    "odo_gear_teeth":    "Odometer driven gear tooth count (1-64).",
    "speed_source":      "Speed signal: 0=off 1=CAN 2=Hall 3=GPS 4=Synthetic 5=OdoTest 6=Serial",
    "rpm_source":        "RPM signal: 0=off 1=CAN 2=coil 3=Synthetic 4=Serial",
    "oil_prs_source":    "Oil pressure: 0=off 1=CAN 2=AV1 3=AV2 4=AV3 5=Synthetic",
    "fuel_prs_source":   "Fuel pressure: 0=off 1=CAN 2=AV1 3=AV2 4=AV3 5=Synthetic",
    "coolant_src":       "Coolant temp: 0=off 1=CAN 2=thermistor 3=Synthetic",
    "oil_temp_src":      "Oil temp: 0=off 1=CAN 2=thermistor",
    "map_source":        "MAP/boost: 0=off 1=CAN 2=AV1 3=AV2 4=AV3 5=Synthetic",
    "lambda_source":     "Lambda/AFR: 0=off 1=CAN 2=AV1 3=AV2 4=AV3",
    "fuel_lvl_src":      "Fuel level: 0=off 1=analog sensor 2=Synthetic",
    "oil_warn_kpa":      "Oil pressure fault threshold kPa (0-1000). Flash below this.",
    "coolant_warn_c":    "Coolant temp fault threshold deg C (50-200). Flash above this.",
    "batt_warn_v":       "Battery voltage fault threshold V (5-16). Flash below this.",
    "engine_run_rpm":    "Min RPM to consider engine running for fault logic (0-2000).",
    "fuel_warn_pct":     "Low fuel warning threshold % (0-50). Flash below this.",
    "clock_offset":      "UTC offset hours (-12 to +12).",
    "fuel_capacity":     "Fuel tank capacity in gallons (1-200).",
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
        self._splash_data: Dict[int, bytes] = {}

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
            ("Speed & Hall",     self._build_tab_speed),
            ("Engine RPM",       self._build_tab_rpm),
            ("Sensors",          self._build_tab_sensors),
            ("CAN Bus",          self._build_tab_can),
            ("LED Tach",         self._build_tab_led_tach),
            ("Display Settings", self._build_tab_display),
            ("Fault Warnings",   self._build_tab_faults),
            ("Clock & Fuel",     self._build_tab_clock),
            ("Splash Screens",   self._build_tab_splash),
        ]
        for title, builder in tabs:
            frame = ttk.Frame(nb)
            nb.add(frame, text=title)
            builder(frame)

    def _param_row(self, parent: tk.Widget, row: int, col: int,
                   label: str, param: str, width: int = 12) -> ttk.Entry:
        lbl = ttk.Label(parent, text=label)
        lbl.grid(row=row, column=col, sticky="e", padx=4, pady=2)
        entry = ttk.Entry(parent, textvariable=self._vars[param], width=width)
        entry.grid(row=row, column=col + 1, sticky="w", padx=4, pady=2)
        if param in TOOLTIPS:
            ToolTip(lbl,   TOOLTIPS[param])
            ToolTip(entry, TOOLTIPS[param])

        def on_change(*_args: object) -> None:
            self._dirty[param] = True
            entry.configure(foreground="blue")

        self._vars[param].trace_add("write", on_change)
        return entry

    # ------ Tab 1: Connection ------
    def _build_tab_connection(self, frame: ttk.Frame) -> None:
        inner = ttk.LabelFrame(frame, text="Serial Connection")
        inner.pack(padx=10, pady=10, fill="both")
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
        self._refresh_ports()

    def _refresh_ports(self) -> None:
        ports = (self._serial.list_ports() if HAS_SERIAL and self._serial else [])
        self._port_combo["values"] = ports
        if ports and not self._port_var.get():
            self._port_var.set(ports[0])

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
            port = self._port_var.get()
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

    # ------ Tab 3: Speed ------
    def _build_tab_speed(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Speed & Hall Sensor")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "Speed source:",   "speed_source")
        self._param_row(grp, 1, 0, "Revs per km:",    "revs_per_km")
        self._param_row(grp, 2, 0, "Teeth per rev:",  "teeth_per_rev")
        self._param_row(grp, 3, 0, "Hall speed min:", "hall_speed_min")
        self._param_row(grp, 4, 0, "Hall filter:",    "filter_hall")
        self._param_row(grp, 5, 0, "Speedo max:",     "speedo_max")

    # ------ Tab 4: RPM ------
    def _build_tab_rpm(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Engine RPM Sensor")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "RPM source:",      "rpm_source")
        self._param_row(grp, 1, 0, "Cylinder count:",  "cyl_count")
        self._param_row(grp, 2, 0, "Debounce (us):",   "rpm_debounce_us")
        self._param_row(grp, 3, 0, "Engine RPM min:",  "engine_rpm_min")
        self._param_row(grp, 4, 0, "RPM filter:",      "filter_rpm")

    # ------ Tab 5: Sensors ------
    def _build_tab_sensors(self, frame: ttk.Frame) -> None:
        nb = ttk.Notebook(frame)
        nb.pack(fill="both", expand=True, padx=4, pady=4)
        therm = ttk.Frame(nb)
        nb.add(therm, text="Thermistor")
        g = ttk.LabelFrame(therm, text="Filter")
        g.pack(padx=10, pady=10, fill="x")
        self._param_row(g, 0, 0, "Therm filter:", "filter_therm")
        fuel = ttk.Frame(nb)
        nb.add(fuel, text="Fuel Level")
        g2 = ttk.LabelFrame(fuel, text="Filter")
        g2.pack(padx=10, pady=10, fill="x")
        self._param_row(g2, 0, 0, "Fuel filter:", "filter_fuel")
        av = ttk.Frame(nb)
        nb.add(av, text="AV1/AV2/AV3")
        g3 = ttk.LabelFrame(av, text="Analog sensor filters")
        g3.pack(padx=10, pady=10, fill="x")
        self._param_row(g3, 0, 0, "AV1 filter:", "filter_av1")
        self._param_row(g3, 1, 0, "AV2 filter:", "filter_av2")
        self._param_row(g3, 2, 0, "AV3 filter:", "filter_av3")
        batt = ttk.Frame(nb)
        nb.add(batt, text="Battery")
        g4 = ttk.LabelFrame(batt, text="Battery voltage sensor")
        g4.pack(padx=10, pady=10, fill="x")
        self._param_row(g4, 0, 0, "Vbatt filter:", "filter_vbatt")
        self._param_row(g4, 1, 0, "Vbatt scaler:", "vbatt_scaler")

    # ------ Tab 6: CAN ------
    def _build_tab_can(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="CAN Bus")
        grp.pack(padx=10, pady=10, fill="x")
        self._param_row(grp, 0, 0, "CAN protocol:", "can_protocol")
        ttk.Label(grp, text="0=Haltech v2  1=Megasquirt  2=AiM  3=OBDII",
                  foreground="gray").grid(row=0, column=2, sticky="w", padx=8)
        src_grp = ttk.LabelFrame(frame, text="Signal sources")
        src_grp.pack(padx=10, pady=6, fill="both")
        for i, (lbl, p) in enumerate([
            ("Oil pressure:",  "oil_prs_source"),
            ("Fuel pressure:", "fuel_prs_source"),
            ("Coolant temp:",  "coolant_src"),
            ("Oil temp:",      "oil_temp_src"),
            ("MAP / boost:",   "map_source"),
            ("Lambda / AFR:",  "lambda_source"),
            ("Fuel level:",    "fuel_lvl_src"),
        ]):
            self._param_row(src_grp, i, 0, lbl, p)

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
            r, c = divmod(i, 3)
            ttk.Label(info, text=name, width=24).grid(row=r, column=c, sticky="w", padx=4, pady=1)

    # ------ Tab 9: Faults ------
    def _build_tab_faults(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Fault Warning Thresholds")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "Oil pressure (kPa):",  "oil_warn_kpa")
        self._param_row(grp, 1, 0, "Coolant temp (deg C):", "coolant_warn_c")
        self._param_row(grp, 2, 0, "Battery voltage (V):", "batt_warn_v")
        self._param_row(grp, 3, 0, "Engine running RPM:",  "engine_run_rpm")
        self._param_row(grp, 4, 0, "Low fuel (%):",        "fuel_warn_pct")

    # ------ Tab 10: Clock & Fuel ------
    def _build_tab_clock(self, frame: ttk.Frame) -> None:
        grp = ttk.LabelFrame(frame, text="Clock & Fuel")
        grp.pack(padx=10, pady=10, fill="both")
        self._param_row(grp, 0, 0, "UTC offset (hours):",       "clock_offset")
        self._param_row(grp, 1, 0, "Fuel tank capacity (gal):", "fuel_capacity")
        odo = ttk.LabelFrame(frame, text="Odometer motor calibration")
        odo.pack(padx=10, pady=6, fill="both")
        self._param_row(odo, 0, 0, "Steps per revolution:", "odo_steps")
        self._param_row(odo, 1, 0, "Motor gear teeth:",     "odo_motor_teeth")
        self._param_row(odo, 2, 0, "Driven gear teeth:",    "odo_gear_teeth")

    # ------ Tab 11: Splash ------
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
            self._set_status(f"Read {len(params)} parameters from device.")
        except Exception as exc:
            messagebox.showerror("Read error", str(exc))

    def _write_to_device(self) -> None:
        if not self._serial or not self._serial.is_connected:
            messagebox.showwarning("Not connected", "Connect to the Arduino first.")
            return
        dirty = [p for p in ALL_PARAMS if self._dirty[p] and self._vars[p].get()]
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
        params = {p: self._vars[p].get() for p in ALL_PARAMS if self._vars[p].get()}
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
            for k, v in params.items():
                if k in self._vars:
                    self._vars[k].set(v)
                    self._dirty[k] = True
            self._set_status(f"Loaded {len(params)} parameters from {os.path.basename(path)}")
        except Exception as exc:
            messagebox.showerror("Load error", str(exc))

    def run(self) -> None:
        self.root.mainloop()


if __name__ == "__main__":
    app = GaugeConfigApp()
    app.run()
