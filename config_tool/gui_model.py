"""
gui_model.py
============
UI-independent model for the Gauge V4 Configuration Tool: option lists, parameter
metadata, unit conversions, validation, config-file migration and device write
ordering. Shared by the web-view front end (api.py) and the legacy Tkinter UI (gui.py).
"""

from __future__ import annotations

import colorsys
import math
from typing import Callable, Dict, Optional

TOOL_VERSION = "0.1"
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
TACH_SWEEP_OPTIONS = ("Left to right", "Right to left", "Edge to center", "Center to edge")
TACH_COLORS = {
    "tach_color_normal": "1E0F00",
    "tach_color_warning": "500A00",
    "tach_color_shift": "500000",
}


def tach_color(value: str) -> str:
    if len(value) != 6 or any(c not in "0123456789abcdefABCDEF" for c in value):
        raise ValueError("Tach colors must be six hexadecimal RGB digits.")
    return value.upper()


def tach_color_hsv(value: str) -> tuple[float, float, float]:
    color = tach_color(value)
    channels = [int(color[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return colorsys.rgb_to_hsv(*channels)


def tach_hsv_color(hue: float, brightness: float) -> str:
    channels = colorsys.hsv_to_rgb(hue % 360 / 360, 1.0, brightness / 100)
    return "".join(f"{round(channel * 255):02X}" for channel in channels)


def tach_display_color(value: str, black_hue: float = 0) -> str:
    """Map LED brightness 0–100% to screen brightness 60–100%."""
    hue, saturation, brightness = tach_color_hsv(value)
    if brightness == 0:
        hue, saturation = black_hue % 360 / 360, 1.0
    channels = colorsys.hsv_to_rgb(hue, saturation, 0.6 + 0.4 * brightness)
    return "#" + "".join(f"{round(channel * 255):02X}" for channel in channels)


def tach_steps(count: int, sweep: int) -> int:
    return count if sweep < 2 else (count + 1) // 2


def tach_led_step(index: int, count: int, sweep: int) -> int:
    if sweep == 0:
        return index
    if sweep == 1:
        return count - 1 - index
    edge = min(index, count - 1 - index)
    return edge if sweep == 2 else tach_steps(count, sweep) - 1 - edge


def tach_step_rpm(step: int, steps: int, minimum: int, maximum: int) -> int:
    if steps <= 1:
        return minimum
    return minimum + (step * (maximum - minimum) + steps - 2) // (steps - 1)


# Order must match the AV_TYPE_* values in firmware config_calibration.h
AV_TYPE_OPTIONS = (
    "2 Bar Absolute Pressure (30 PSIA)",
    "3 Bar Absolute Pressure (43.5 PSIA)",
    "100 PSI Gauge Pressure",
    "None (raw voltage)",
    "Custom",
    "Lambda",
)
AV_TYPE_PARAMS = ("av1_type", "av2_type", "av3_type")
AV_TYPE_NONE = 3
AV_TYPE_CUSTOM = 4
AV_TYPE_LAMBDA = 5
ATMOSPHERE_KPA = 101.3
ADC_COUNTS = 1023
ADC_REF_VOLTS = 5.0
# Preset calibration points: (ADC low, ADC high, physical low, physical high) in firmware units
# (absolute kPa*10). All presets are 0.5-4.5V sensors.
AV_PRESETS = {
    0: (102, 921, 0, 2068),
    1: (102, 921, 0, 3000),
    2: (102, 921, round(ATMOSPHERE_KPA * 10), round((ATMOSPHERE_KPA + 689.5) * 10)),
    # Generic 0-5V wideband: 0 V = 0.5 Lambda, 5 V = 1.52 Lambda (Lambda*1000)
    5: (0, 1023, 500, 1520),
}
# (adc_lo, adc_hi, phys_lo, phys_hi) firmware parameter names for AV1-AV3
AV_CAL_PARAMS = tuple(
    tuple(f"av{n}_{field}" for field in ("adc_lo", "adc_hi", "phys_lo", "phys_hi"))
    for n in (1, 2, 3)
)
# Stored raw = physical value * scale (pressure is stored as absolute kPa * 10)
PHYS_UNIT_SCALE = {"kPa": 10, "Lambda": 1000, "PSI (abs)": 10 / 0.1450377377}


def av_physical_unit(av_type: Optional[int], imperial: bool = False) -> str:
    """Unit the user enters physical values in for an AV input of the given type."""
    if av_type == AV_TYPE_LAMBDA:
        return "Lambda"
    return "PSI (abs)" if imperial else "kPa"

def volts_to_adc(value: str) -> int:
    try:
        volts = float(value)
    except ValueError as exc:
        raise ValueError("Voltage must be a number from 0 to 5 V.") from exc
    if not math.isfinite(volts) or not 0 <= volts <= ADC_REF_VOLTS:
        raise ValueError("Voltage must be from 0 to 5 V.")
    return round(volts * ADC_COUNTS / ADC_REF_VOLTS)


def adc_to_volts(adc: int) -> float:
    return adc * ADC_REF_VOLTS / ADC_COUNTS


def physical_to_raw(value: str, unit: str) -> int:
    try:
        phys = float(value)
    except ValueError as exc:
        raise ValueError(f"Physical value must be a number in {unit}.") from exc
    raw = round(phys * PHYS_UNIT_SCALE[unit]) if math.isfinite(phys) else 32768
    if not -32768 <= raw <= 32767:
        raise ValueError(f"Physical value is out of range for {unit}.")
    return raw


def raw_to_physical(raw: int, unit: str) -> float:
    return raw / PHYS_UNIT_SCALE[unit]


def _trim_number(value: float, decimals: int = 3) -> str:
    return f"{value:.{decimals}f}".rstrip("0").rstrip(".")


# Thermistor and fuel sender curves. The sensor is wired to ground with a bias resistor to the
# 5 V reference, so V = 5 * R / (R + Rbias). The firmware stores curve voltages in millivolts.
SENSOR_REF_MV = 5000
THERM_POINTS = 6
THERM_TYPE_OPTIONS = ("GM CLT", "Bosch NTC M12", "Custom")
THERM_TYPE_CUSTOM = 2
# (temperature degC, resistance ohm), ordered by ascending resistance
THERM_PRESETS = {
    0: ((150, 48), (105, 154), (75, 395), (25, 2795), (-5, 12261), (-40, 100865)),
    1: ((130, 89), (100, 187), (70, 436), (30, 1707), (-10, 9397), (-40, 45313)),
}
THERM_TEMP_PARAMS = tuple(f"therm_t{i}" for i in range(THERM_POINTS))
THERM_RES_PARAMS = tuple(f"therm_r{i}" for i in range(THERM_POINTS))
THERM_VOLT_PARAMS = tuple(f"therm_v{i}" for i in range(THERM_POINTS))
# Order must match the fuel sender options in firmware config_calibration.h (FUEL_TYPE_CUSTOM)
FUEL_TYPE_OPTIONS = (
    "Ford to 1986 - 73-10 Ohms",
    "Ford 1987+ - 16-158 Ohms",
    "GM to 1964 - 0-30 Ohms",
    "GM 1965-1997 - 0-90 Ohms",
    "GM 1998+ - 40-250 Ohms",
    "Mopar to 1986 - 73-10 Ohms",
    "Custom",
)
FUEL_TYPE_CUSTOM = 6
# (resistance at empty, resistance at full) in ohms
FUEL_PRESETS = {0: (73, 10), 1: (16, 158), 2: (0, 30), 3: (0, 90), 4: (40, 250), 5: (73, 10)}
FUEL_DEFAULT_MID_PCT = 50
CURVE_INPUT_PARAMS = (
    "therm_bias", *THERM_TEMP_PARAMS, *THERM_RES_PARAMS,
    "fuel_bias", "fuel_r_empty", "fuel_r_mid", "fuel_r_full", "fuel_mid_pct",
)
CURVE_PARAMS = (
    "therm_type", *CURVE_INPUT_PARAMS, *THERM_VOLT_PARAMS,
    "fuel_type", "fuel_v_empty", "fuel_v_mid", "fuel_v_full",
)


def divider_millivolts(resistance: int, bias: int) -> int:
    """Voltage across a sensor to ground with a bias resistor to the 5 V reference."""
    if bias <= 0 or resistance < 0:
        raise ValueError("Bias must be above 0 ohms and resistance cannot be negative.")
    return round(SENSOR_REF_MV * resistance / (resistance + bias))


def thermistor_curve(points, bias: int):
    """Return [(temp, resistance, millivolts)] sorted by ascending voltage.

    Raises ValueError when two points end up with the same voltage.
    """
    rows = sorted(
        ((int(temp), int(res), divider_millivolts(int(res), bias)) for temp, res in points),
        key=lambda row: row[1],
    )
    if any(b[2] <= a[2] for a, b in zip(rows, rows[1:])):
        raise ValueError("Thermistor resistances must be distinct and produce different voltages.")
    return rows


# Switec Z27.168: 3 steps per degree, driven with 4 microsteps per step
MOTOR_STEPS_PER_DEGREE = 3 * 4
MOTOR_MAX_ANGLE = 315  # Switec Z27.168 mechanical limit, degrees
MOTOR_SWEEP_STEPS_RANGE = (100, MOTOR_MAX_ANGLE * MOTOR_STEPS_PER_DEGREE)
UNIT_SYSTEM_OPTIONS = ("Metric", "'Merican")
# Order must match the MOTOR_FUNC_* values in firmware config_calibration.h
MOTOR_FUNCTION_OPTIONS = (
    "Off", "Fuel level", "Coolant temp", "RPM", "Vehicle speed",
    "Oil pressure", "Fuel pressure", "Oil temp", "MAP", "Lambda (x1000)",
)
MOTOR_FUNCTION_PARAMS = ("m1_func", "m2_func", "m3_func", "m4_func")
# Gauge min/max units per MOTOR_FUNC_* value, and the min/max applied when a function is picked
MOTOR_FUNCTION_UNITS = (
    "", "%", "\u00b0C", "RPM", "km/h", "kPa", "kPa", "\u00b0C", "kPa",
    "\u03bb\u00d71000",
)
MOTOR_FUNCTION_RANGES = (
    (0, 0), (10, 100), (50, 115), (0, 8000), (0, 240),
    (0, 700), (0, 700), (0, 150), (0, 300), (700, 1300),
)
# (function, min, max) parameter names for each gauge motor. CW stores low in min and high in max;
# CCW stores them swapped, which reverses the needle's direction of travel.
MOTOR_PARAMS = (
    ("m1_func", "m1_min", "m1_max"),
    ("m2_func", "m2_min", "m2_max"),
    ("m3_func", "m3_min", "m3_max"),
    ("m4_func", "m4_min", "m4_max"),
)
DIRECTION_OPTIONS = ("CW", "CCW")

# ---- Low side outputs (ULN2003 on D12 = Output 1, D13 = Output 2) ----
OUTPUT_COUNT = 2
OUTPUT_PWM_POINTS = 8
OUTPUT_FREQ_RANGE = (20, 500)
OUTPUT_INT16_RANGE = (-32768, 32767)
OUTPUT_MODE_OPTIONS = ("Disabled", "On/Off", "PWM")  # index = firmware LSO_MODE_*
OUTPUT_MODE_DISABLED, OUTPUT_MODE_ON_OFF, OUTPUT_MODE_PWM = 0, 1, 2
OUTPUT_ON_OPS = (">", "<")  # index = firmware on_op; the off operator is always the other one
# Firmware parameter values are MOTOR_FUNC_* 1-9 plus OUT_PARAM_BATTERY (10): value = index + 1
OUTPUT_PARAM_OPTIONS = (*MOTOR_FUNCTION_OPTIONS[1:], "Battery voltage (x10)")
OUTPUT_PARAM_UNITS = (*MOTOR_FUNCTION_UNITS[1:], "V×10")
OUTPUT_PARAM_RANGES = (*MOTOR_FUNCTION_RANGES[1:], (100, 150))
# Conversion kind per output parameter (None = same in both unit systems)
OUTPUT_PARAM_KINDS = (None, "temp", None, "speed", "pressure", "pressure", "temp", "pressure_abs", None, None)

# ---- Fault warning debounce: entered in seconds, stored in ms (firmware *_WARN_DEBOUNCE_MS) ----
WARN_DEBOUNCE_MAX_S = 60
WARN_DEBOUNCE_PARAMS = ("oil_warn_ms", "coolant_warn_ms", "batt_warn_ms", "fuel_warn_ms")
WARN_DEBOUNCE_NAMES = {
    "oil_warn_ms": "Oil pressure", "coolant_warn_ms": "Engine temp",
    "batt_warn_ms": "Battery voltage", "fuel_warn_ms": "Low fuel",
}


def debounce_seconds_to_ms(text: str) -> int:
    """Seconds as typed (0-60, ms resolution) -> stored milliseconds. Raises ValueError."""
    try:
        seconds = float(str(text).strip())
    except ValueError:
        raise ValueError(f"Delay must be a number of seconds from 0 to {WARN_DEBOUNCE_MAX_S}.") from None
    if not math.isfinite(seconds) or not 0 <= seconds <= WARN_DEBOUNCE_MAX_S:
        raise ValueError(f"Delay must be from 0 to {WARN_DEBOUNCE_MAX_S} seconds.")
    return round(seconds * 1000)


def debounce_ms_to_seconds(text: str) -> str:
    try:
        return _trim_number(int(text) / 1000, 3)
    except ValueError:
        return text


# ---- Tool-wide display unit system (the firmware always stores metric) ----
UNIT_SYSTEMS = ("metric", "imperial")
# Unit chosen for each older per-field unit setting under each system
FIELD_UNITS = {
    "metric":   {"speedo_max_units": "km/h", "oil_warn_units": "kPa", "coolant_warn_units": "C",
                 "fuel_capacity_units": "L"},
    "imperial": {"speedo_max_units": "mph", "oil_warn_units": "PSI", "coolant_warn_units": "F",
                 "fuel_capacity_units": "gal"},
}
IMPERIAL_UNIT_LABELS = {"temp": "°F", "speed": "mph", "pressure": "PSI", "pressure_abs": "PSI (abs)"}
METRIC_UNIT_LABELS = {"temp": "°C", "speed": "km/h", "pressure": "kPa", "pressure_abs": "kPa"}


def unit_label(kind: Optional[str], imperial: bool, metric_label: str = "") -> str:
    if kind is None:
        return metric_label
    return (IMPERIAL_UNIT_LABELS if imperial else METRIC_UNIT_LABELS)[kind]


def raw_to_display(text: str, kind: Optional[str], imperial: bool) -> str:
    """Stored metric integer -> text in the selected unit system (non-numbers pass through)."""
    if not imperial or kind is None:
        return text
    try:
        raw = int(text)
    except ValueError:
        return text
    if kind == "temp":
        value = raw * 9 / 5 + 32
    elif kind == "speed":
        value = raw / 1.609344
    else:
        value = raw * 0.1450377377
    return _trim_number(value, 1)


def display_to_raw(text: str, kind: Optional[str], imperial: bool) -> int:
    """Text in the selected unit system -> stored metric integer. Raises ValueError."""
    text = text.strip()
    if not imperial or kind is None:
        return int(text)
    value = float(text)
    if not math.isfinite(value):
        raise ValueError("not a number")
    if kind == "temp":
        return round((value - 32) * 5 / 9)
    if kind == "speed":
        return round(value * 1.609344)
    return round(value / 0.1450377377)


def output_param_names(n: int) -> tuple:
    """Config parameter names for output n (1-2), with the mode last so it is written last."""
    p = f"out{n}_"
    return (
        p + "param", p + "on_op", p + "on_val", p + "off_val", p + "freq",
        *(f"{p}x{i}" for i in range(OUTPUT_PWM_POINTS)),
        *(f"{p}d{i}" for i in range(OUTPUT_PWM_POINTS)),
        p + "mode",
    )


OUTPUT_PARAMS = tuple(p for n in range(1, OUTPUT_COUNT + 1) for p in output_param_names(n))


def output_off_operator(on_op: int) -> str:
    """The off comparison is the opposite of the on comparison."""
    return OUTPUT_ON_OPS[1 - on_op]


def _output_int(text: str, name: str, low: int, high: int) -> int:
    try:
        value = int(str(text).strip())
    except ValueError:
        raise ValueError(f"{name} must be a whole number.") from None
    if not low <= value <= high:
        raise ValueError(f"{name} must be from {low} to {high}.")
    return value


def validate_on_off(on_op: str, on_val: str, off_val: str, fmt: Callable[[int], str] = str) -> None:
    """On/off thresholds must leave a hysteresis band on the off side of the on value.

    fmt formats a stored value for messages (e.g. in the selected display units).
    """
    op = _output_int(on_op, "On comparison", 0, 1)
    on = _output_int(on_val, "On value", *OUTPUT_INT16_RANGE)
    off = _output_int(off_val, "Off value", *OUTPUT_INT16_RANGE)
    if op == 0 and not off < on:
        raise ValueError(f"Turns on above {fmt(on)}, so the off value must be below {fmt(on)}.")
    if op == 1 and not off > on:
        raise ValueError(f"Turns on below {fmt(on)}, so the off value must be above {fmt(on)}.")


def validate_pwm_curve(freq: str, xs: list, duties: list, fmt: Callable[[int], str] = str) -> None:
    """Frequency 20-500 Hz, parameter values strictly ascending, duty 0-100 %."""
    _output_int(freq, "PWM frequency (Hz)", *OUTPUT_FREQ_RANGE)
    values = [_output_int(x, f"Point {i + 1} value", *OUTPUT_INT16_RANGE) for i, x in enumerate(xs)]
    for i, d in enumerate(duties):
        _output_int(d, f"Point {i + 1} duty (%)", 0, 100)
    for i in range(1, len(values)):
        if values[i] <= values[i - 1]:
            raise ValueError(
                f"Point {i + 1} value ({fmt(values[i])}) must be greater than point {i} ({fmt(values[i - 1])})."
            )


def validate_outputs(values: Dict[str, str], imperial: bool = False) -> None:
    """Validate every configured output; raises ValueError naming the output.

    Values are stored metric integers; messages show them in the selected unit system.
    """
    pwm_freqs = []
    for n in range(1, OUTPUT_COUNT + 1):
        p = f"out{n}_"
        mode_text = values.get(p + "mode", "")
        if mode_text == "":
            continue  # Not read from a device or file yet
        try:
            mode = _output_int(mode_text, "Mode", 0, len(OUTPUT_MODE_OPTIONS) - 1)
            if mode == OUTPUT_MODE_DISABLED:
                continue
            param = _output_int(values.get(p + "param", ""), "Parameter", 1, len(OUTPUT_PARAM_OPTIONS))
            kind = OUTPUT_PARAM_KINDS[param - 1]
            unit = unit_label(kind, imperial, OUTPUT_PARAM_UNITS[param - 1])

            def fmt(raw: int, kind: Optional[str] = kind, unit: str = unit) -> str:
                return f"{raw_to_display(str(raw), kind, imperial)} {unit}".strip()

            if mode == OUTPUT_MODE_ON_OFF:
                validate_on_off(values.get(p + "on_op", ""), values.get(p + "on_val", ""),
                                values.get(p + "off_val", ""), fmt)
            else:
                validate_pwm_curve(
                    values.get(p + "freq", ""),
                    [values.get(f"{p}x{i}", "") for i in range(OUTPUT_PWM_POINTS)],
                    [values.get(f"{p}d{i}", "") for i in range(OUTPUT_PWM_POINTS)],
                    fmt,
                )
                pwm_freqs.append(int(values[p + "freq"]))
        except ValueError as exc:
            raise ValueError(f"Output {n}: {exc}") from None
    if len(set(pwm_freqs)) > 1:
        raise ValueError(
            "Both outputs are PWM and share one hardware timer, so they must use the same frequency."
        )


def output_param_active(param: str, values: Dict[str, str]) -> bool:
    """False for output settings the selected mode does not use (they are not written to the device)."""
    if not param.startswith("out") or param not in OUTPUT_PARAMS:
        return True
    field = param[5:]
    mode = values.get(param[:5] + "mode", "")
    if field in ("on_op", "on_val", "off_val"):
        return mode == str(OUTPUT_MODE_ON_OFF)
    if field == "freq" or field[0] in "xd":
        return mode == str(OUTPUT_MODE_PWM)
    return True


def pwm_duty_permille(xs: list, duties: list, value: int) -> int:
    """Mirror of firmware lsoPwmDutyPermille(): duty in 0.1 % steps, clamped at the curve ends."""
    if value <= xs[0]:
        return duties[0] * 10
    for i in range(len(xs) - 1):
        if value < xs[i + 1]:
            span = xs[i + 1] - xs[i]
            if span <= 0:
                return duties[i + 1] * 10
            d0, d1 = duties[i] * 10, duties[i + 1] * 10
            return d0 + int((value - xs[i]) * (d1 - d0) / span)  # C truncates toward zero
    return duties[-1] * 10


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
    "filter_gps": (("speed_source", 3),),
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
FILTER_PARAMS = (
    "filter_vbatt", "filter_fuel", "filter_therm", "filter_av1",
    "filter_av2", "filter_av3", "filter_hall", "filter_gps", "filter_rpm",
)
KMH_PER_MPH = 1.609344
PSI_PER_KPA = 0.1450377377
LITERS_PER_GALLON = 3.785411784


def _filter_percent_to_raw(percent: int) -> int:
    if not 0 <= percent <= 99:
        raise ValueError("Filter percentage must be between 0 and 99.")
    return min(127, (percent * 128 + 50) // 100)


def _filter_raw_to_percent(raw: int) -> int:
    if not 0 <= raw <= 127:
        raise ValueError("Filter coefficient must be between 0 and 127.")
    return min(99, (raw * 100 + 64) // 128)


def _decode_config_filters(params: Dict[str, str]) -> Dict[str, str]:
    marker = params.get("filter_format")
    if marker not in (None, "7"):
        raise ValueError(f"Unsupported filter format: {marker}")
    result: Dict[str, str] = {}
    for param in FILTER_PARAMS:
        if param not in params:
            continue
        raw = int(params[param])
        if marker == "7":
            _filter_raw_to_percent(raw)
            result[param] = str(raw)
            continue
        if param == "filter_gps":
            raise ValueError("Legacy config cannot specify a GPS filter.")
        if param.startswith("filter_av"):
            index = param[-1]
            raw_voltage = params.get(f"av{index}_type", "2") == str(AV_TYPE_NONE)
            denominator = 64 if raw_voltage else 16
            if not 1 <= raw <= 255:
                raise ValueError(f"Invalid legacy {param} coefficient: {params[param]}")
            if not raw_voltage and raw > 16:
                raw = 16  # The old mapped-sensor path capped its coefficient.
        else:
            denominator = {
                "filter_vbatt": 64, "filter_fuel": 64, "filter_therm": 100,
                "filter_hall": 256, "filter_rpm": 256,
            }[param]
        if not 1 <= raw <= denominator:
            raise ValueError(f"Invalid legacy {param} coefficient: {params[param]}")
        retained = ((denominator - raw) * 128 + denominator // 2) // denominator
        result[param] = str(min(127, retained))
    return result


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
    "av1_type", "av2_type", "av3_type",
    *[param for cal in AV_CAL_PARAMS for param in cal],
    *CURVE_PARAMS,
    "revs_per_km", "teeth_per_rev", "filter_hall", "filter_gps", "hall_speed_min",
    "cyl_count", "filter_rpm", "rpm_debounce_us", "engine_rpm_min",
    "speedo_max",
    "num_leds", "warn_leds", "shift_leds", "tach_sweep", "tach_max", "tach_min", "tach_flash",
    *TACH_COLORS,
    "odo_steps", "odo_motor_teeth", "odo_gear_teeth",
    "speed_source", "rpm_source", "oil_prs_source", "fuel_prs_source",
    "coolant_src", "oil_temp_src", "map_source", "lambda_source", "fuel_lvl_src",
    "oil_warn_kpa", "coolant_warn_c", "batt_warn_v", "engine_run_rpm", "fuel_warn_pct",
    *WARN_DEBOUNCE_PARAMS,
    "clock_offset", "fuel_capacity", "can_protocol", "units",
    "disp1_order", "disp2_order",
    *OUTPUT_PARAMS,
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
    "m1_min":            "Motor 1 signal value at the zero end of the dial; units depend on the selected function.",
    "m1_max":            "Motor 1 signal value at the full end of the dial; units depend on the selected function.",
    "m2_min":            "Motor 2 signal value at the zero end of the dial; units depend on the selected function.",
    "m2_max":            "Motor 2 signal value at the full end of the dial; units depend on the selected function.",
    "m3_min":            "Motor 3 signal value at the zero end of the dial; units depend on the selected function.",
    "m3_max":            "Motor 3 signal value at the full end of the dial; units depend on the selected function.",
    "m4_min":            "Motor 4 signal value at the zero end of the dial; units depend on the selected function.",
    "m4_max":            "Motor 4 signal value at the full end of the dial; units depend on the selected function.",
    "motor_direction":   "Direction the needle sweeps as the value rises. CCW swaps the min and max stored in the config.",
    "ms_zero_delay":     "Motor S zeroing step delay us (10-5000).",
    "ms_zero_factor":    "Fraction of MS_SWEEP for Motor S zeroing (0.1-1.0).",
    "motor_sweep_ms":    "Startup test sweep duration ms (100-5000).",
    "filter_vbatt":      "Battery voltage filtering: 0% off, 99% heavy.",
    "vbatt_scaler":      "Voltage divider scale factor for battery ADC (0.001-0.1).",
    "filter_fuel":       "Fuel sensor filtering: 0% off, 99% heavy.",
    "filter_therm":      "Thermistor filtering: 0% off, 99% heavy.",
    "filter_av1":        "AV1 filtering: 0% off, 99% heavy.",
    "filter_av2":        "AV2 filtering: 0% off, 99% heavy.",
    "filter_av3":        "AV3 filtering: 0% off, 99% heavy.",
    "av1_type":          "Sensor type on AV1. Presets fill the calibration points; editing them switches to Custom.",
    "av2_type":          "Sensor type on AV2. Presets fill the calibration points; editing them switches to Custom.",
    "av3_type":          "Sensor type on AV3. Presets fill the calibration points; editing them switches to Custom.",
    "av1_adc_lo":     "AV1 voltage low calibration point, in volts (0-5). Stored as ADC counts.",
    "av1_adc_hi":     "AV1 voltage high calibration point, in volts (0-5). Stored as ADC counts.",
    "av1_phys_lo":    "AV1 physical value low calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "av1_phys_hi":    "AV1 physical value high calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "av2_adc_lo":     "AV2 voltage low calibration point, in volts (0-5). Stored as ADC counts.",
    "av2_adc_hi":     "AV2 voltage high calibration point, in volts (0-5). Stored as ADC counts.",
    "av2_phys_lo":    "AV2 physical value low calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "av2_phys_hi":    "AV2 physical value high calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "av3_adc_lo":     "AV3 voltage low calibration point, in volts (0-5). Stored as ADC counts.",
    "av3_adc_hi":     "AV3 voltage high calibration point, in volts (0-5). Stored as ADC counts.",
    "av3_phys_lo":    "AV3 physical value low calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "av3_phys_hi":    "AV3 physical value high calibration point, in kPa (absolute) or Lambda. Stored as kPa*10 or Lambda*1000.",
    "therm_type":        "Thermistor curve. Presets fill the table; editing the table switches to Custom.",
    "therm_bias":        "Thermistor bias resistor to +5 V, in ohms.",
    "fuel_type":         "Fuel sender type. Presets fill the empty and full resistances; editing them switches to Custom.",
    "fuel_bias":         "Fuel sender bias resistor to +5 V, in ohms.",
    "fuel_r_empty":      "Fuel sender resistance at empty, in ohms.",
    "fuel_r_full":       "Fuel sender resistance at full, in ohms.",
    "fuel_v_empty":      "Computed sender voltage at empty, in mV.",
    "fuel_v_full":       "Computed sender voltage at full, in mV.",
    "fuel_r_mid":        "Fuel sender resistance at the mid point, in ohms.",
    "fuel_v_mid":        "Computed sender voltage at the mid point, in mV.",
    "fuel_mid_pct":      "Fuel level at the mid point, in percent (1-99). Fuel level is measured in percent of capacity.",
    **{f"therm_t{i}": f"Thermistor point {i + 1} temperature, in the selected units (stored as whole degC)."
       for i in range(THERM_POINTS)},
    **{f"therm_r{i}": f"Thermistor point {i + 1} resistance, in ohms." for i in range(THERM_POINTS)},
    **{f"therm_v{i}": f"Computed thermistor point {i + 1} voltage, in mV." for i in range(THERM_POINTS)},
    "revs_per_km":       "VSS shaft revolutions per km (100-10000).",
    "teeth_per_rev":     "VSS teeth per shaft revolution (1-64).",
    "filter_hall":       "Hall speed filtering: 0% off, 99% heavy.",
    "filter_gps":        "GPS speed filtering: 0% off, 99% heavy.",
    "hall_speed_min":    "Minimum speed cutoff in km/h (0-1).",
    "cyl_count":         "Engine cylinder count (2-16).",
    "filter_rpm":        "RPM filtering: 0% off, 99% heavy.",
    "rpm_debounce_us":   "RPM pulse debounce window us (100-20000).",
    "engine_rpm_min":    "Minimum displayed RPM (0-255).",
    "speedo_max":        "Maximum speedometer reading in the selected unit.",
    "num_leds":          "Total LED strip count (1-64).",
    "warn_leds":         "Warning start, in LED steps before the final step.",
    "shift_leds":        "Shift start, in LED steps before the final step.",
    "tach_sweep":        "Sweep direction; LED 0 is on the driver's left.",
    "tach_max":          "RPM at which the last LED step illuminates (1000-15000).",
    "tach_flash":        "RPM at which illuminated shift-zone LEDs flash (0-15000).",
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
    **{
        param: f"{name} warning delay, in seconds (0-{WARN_DEBOUNCE_MAX_S}). The reading must stay past "
               "its threshold this long before the warning activates; it clears as soon as it recovers."
        for param, name in WARN_DEBOUNCE_NAMES.items()
    },
    "clock_offset":      "UTC offset hours (-12 to +12).",
    "fuel_capacity":     "Fuel tank capacity in gallons or liters (1-200 gallons).",
    "can_protocol":      "Select the CAN protocol used by your vehicle.",
    "units":             "Unit system shown on the gauge: Metric or 'Merican (imperial).",
    "disp1_order":       "Screens reachable by scrolling display 1, in order (Settings is always last).",
    "disp2_order":       "Screens selectable for display 2 from the Settings menu, in order.",
    **{
        f"out{n}_{field}": text.format(n=n)
        for n in range(1, OUTPUT_COUNT + 1)
        for field, text in (
            ("mode", "Output {n}: Disabled holds it off; On/Off switches with hysteresis; PWM follows the curve."),
            ("param", "Output {n}: signal that drives the output. Units match the Motor Config tab."),
            ("on_op", "Output {n}: turn on when the value is above (>) or below (<) the on value."),
            ("on_val", "Output {n}: value at which the output turns on."),
            ("off_val", "Output {n}: value at which the output turns off. Must be on the off side of the on value."),
            ("freq", "Output {n}: PWM frequency, 20-500 Hz. Shared with the other output when both are PWM."),
        )
    },
    **{
        f"out{n}_x{i}": f"Output {n}: point {i + 1} parameter value. Values must increase left to right."
        for n in range(1, OUTPUT_COUNT + 1) for i in range(OUTPUT_PWM_POINTS)
    },
    **{
        f"out{n}_d{i}": f"Output {n}: point {i + 1} duty cycle, 0-100 %."
        for n in range(1, OUTPUT_COUNT + 1) for i in range(OUTPUT_PWM_POINTS)
    },
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


# ---------------------------------------------------------------------------
# Whole-configuration helpers used by the web-view bridge (api.py).
# Each mirrors the matching GaugeConfigApp handler in gui.py, but works on a plain
# {param: stored value} dict instead of Tk variables.
# ---------------------------------------------------------------------------

LOAD_SKIP_KEYS = (
    "config_format", "filter_format", "speedo_max_units",
    "oil_warn_units", "coolant_warn_units", "fuel_capacity_units",
)
MOTOR_TITLES = ("Motor 1", "Motor 2", "Motor 3", "Motor 4")
MOTOR_SWEEP_PARAMS = ("m1_sweep", "m2_sweep", "m3_sweep", "m4_sweep")

# Test tab signals: (key, label, metric unit, imperial unit, metric range, imperial range,
# default metric value, serial method, source param). Unit conversion happens in the UI.
TEST_SIGNALS = (
    ("speed", "Speed", "km/h", "mph", (0, 300), (0, 186), 0, "test_speed", "speed_source"),
    ("rpm", "RPM", "rpm", "rpm", (0, 10000), (0, 10000), 0, "test_rpm", "rpm_source"),
    ("oil", "Oil pressure", "kPa", "psi", (0, 1000), (0, 145), 0, "test_oil_pressure", "oil_prs_source"),
    ("coolant", "Coolant temp", "°C", "°F", (-40, 150), (-40, 300), 20,
     "test_coolant_temp", "coolant_src"),
    ("fuel", "Fuel level", "%", "%", (0, 100), (0, 100), 50, "test_fuel_level", "fuel_lvl_src"),
)


def selected_sources(values: Dict[str, str]) -> Dict[str, str]:
    return {param: values.get(param, "") for param in SOURCE_OPTIONS}


def tach_values(values: Dict[str, str]) -> tuple:
    """(count, min, max, sweep, warning, shift) after the checks in GaugeConfigApp._tach_values."""
    try:
        count, minimum, maximum, sweep, warning, shift = (
            int(values.get(p, "")) for p in
            ("num_leds", "tach_min", "tach_max", "tach_sweep", "warn_leds", "shift_leds")
        )
    except ValueError as exc:
        raise ValueError("Read or enter all tach settings before adjusting the sliders.") from exc
    if not (1 <= count <= 64 and 0 <= minimum <= 5000 and
            1000 <= maximum <= 15000 and minimum <= maximum and 0 <= sweep <= 3):
        raise ValueError("Check LED count, RPM limits, and sweep style.")
    if not 0 <= shift <= warning < tach_steps(count, sweep):
        raise ValueError("Zone settings exceed the strip or warning starts after shift.")
    return count, minimum, maximum, sweep, warning, shift


def tach_preview_payload(values: Dict[str, str]) -> str:
    count, _minimum, _maximum, sweep, warning, shift = tach_values(values)
    colors = "".join(tach_color(values.get(param, "")) for param in TACH_COLORS)
    return f"{count:02X}{sweep:X}{warning:02X}{shift:02X}{colors}"


def _curve_int(values: Dict[str, str], param: str, name: str, low: int, high: int) -> int:
    try:
        value = int(values.get(param, ""))
    except ValueError:
        raise ValueError(f"{name} must be a whole number.") from None
    if not low <= value <= high:
        raise ValueError(f"{name} must be from {low} to {high}.")
    return value


def compute_curves(values: Dict[str, str], dirty) -> tuple:
    """Mirror of GaugeConfigApp._validate_curves.

    Returns (updates, newly_dirty): the thermistor rows sorted by voltage and the curve
    voltages the firmware needs. Raises ValueError for an invalid curve.
    """
    updates: Dict[str, str] = {}
    newly_dirty: set = set()

    def put(param: str, value: int, force_dirty: bool) -> None:
        text = str(value)
        if values.get(param, "") != text:
            updates[param] = text
            force_dirty = True
        if force_dirty:
            newly_dirty.add(param)

    therm_inputs = ("therm_bias", *THERM_TEMP_PARAMS, *THERM_RES_PARAMS)
    if all(values.get(p, "") for p in therm_inputs):
        bias = _curve_int(values, "therm_bias", "Thermistor bias resistor", 1, 65535)
        points = [
            (_curve_int(values, THERM_TEMP_PARAMS[i], f"Thermistor temperature {i + 1}", -273, 1000),
             _curve_int(values, THERM_RES_PARAMS[i], f"Thermistor resistance {i + 1}", 0, 2000000000))
            for i in range(THERM_POINTS)
        ]
        rows = thermistor_curve(points, bias)
        edited = any(p in dirty for p in ("therm_type", *therm_inputs))
        for i, (temp, res, mv) in enumerate(rows):
            put(THERM_TEMP_PARAMS[i], temp, edited)
            put(THERM_RES_PARAMS[i], res, edited)
            put(THERM_VOLT_PARAMS[i], mv, edited)
    fuel_inputs = ("fuel_bias", "fuel_r_empty", "fuel_r_mid", "fuel_r_full", "fuel_mid_pct")
    if all(values.get(p, "") for p in fuel_inputs):
        bias = _curve_int(values, "fuel_bias", "Fuel bias resistor", 1, 65535)
        empty = _curve_int(values, "fuel_r_empty", "Fuel empty resistance", 0, 65535)
        mid = _curve_int(values, "fuel_r_mid", "Fuel mid resistance", 0, 65535)
        full = _curve_int(values, "fuel_r_full", "Fuel full resistance", 0, 65535)
        _curve_int(values, "fuel_mid_pct", "Fuel mid level", 1, 99)
        v_empty, v_mid, v_full = (divider_millivolts(r, bias) for r in (empty, mid, full))
        if not (v_empty < v_mid < v_full or v_empty > v_mid > v_full):
            raise ValueError(
                "Fuel empty, mid and full resistances must give steadily rising or falling voltages."
            )
        edited = any(p in dirty for p in ("fuel_type", *fuel_inputs))
        put("fuel_v_empty", v_empty, edited)
        put("fuel_v_mid", v_mid, edited)
        put("fuel_v_full", v_full, edited)
    return updates, newly_dirty


def float_text(value: float) -> str:
    """Stored text for float parameters (oil/coolant warnings, fuel capacity), as gui.py writes it."""
    return f"{value:.2f}".rstrip("0").rstrip(".")


class ConfigInvalid(ValueError):
    """A configuration check failed; *title* names the check (shown as the dialog title)."""

    def __init__(self, title: str, message: str) -> None:
        super().__init__(message)
        self.title = title


def validate_config(values: Dict[str, str], dirty, imperial: bool = False) -> tuple:
    """Run every check GaugeConfigApp runs before writing or saving, in the same order.

    *values* are stored (metric) values and *dirty* is the set of edited parameters.
    Returns (updates, newly_dirty) from the curve computation. Raises ConfigInvalid.
    """
    def fail(title: str, exc: Exception) -> None:
        raise ConfigInvalid(title, str(exc)) from None

    def get(param: str) -> str:
        return values.get(param, "")

    # LED tach (_validate_tach)
    if any(get(p) for p in ("num_leds", "tach_min", "tach_max", "tach_sweep")):
        try:
            tach_values(values)
            try:
                flash = int(get("tach_flash"))
            except ValueError as exc:
                raise ValueError("Flash RPM must be an integer from 0 to 15000.") from exc
            if not 0 <= flash <= 15000:
                raise ValueError("Flash RPM must be an integer from 0 to 15000.")
            for param in TACH_COLORS:
                tach_color(get(param))
        except ValueError as exc:
            fail("Invalid LED tach settings", exc)

    # Motor sweep angles (_validate_motor_angles); the UI stores degrees as steps
    for title, param in zip(MOTOR_TITLES, MOTOR_SWEEP_PARAMS):
        text = get(param).strip()
        if not text:
            continue
        try:
            steps = int(text)
        except ValueError:
            fail("Invalid sweep angle", ValueError(f"{title}: Sweep angle must be a number."))
        if not MOTOR_SWEEP_STEPS_RANGE[0] <= steps <= MOTOR_SWEEP_STEPS_RANGE[1]:
            lo = MOTOR_SWEEP_STEPS_RANGE[0] / MOTOR_STEPS_PER_DEGREE
            fail("Invalid sweep angle", ValueError(
                f"{title}: Sweep angle must be between {lo:.1f} and {MOTOR_MAX_ANGLE} degrees."))

    # Speed inputs (_validate_speed_inputs); stored km/h*100 and mph*100
    sources = selected_sources(values)
    try:
        if _source_uses_parameter("hall_speed_min", sources):
            text = get("hall_speed_min")
            if text:
                try:
                    _hall_speed_min_to_raw(str(int(text) / 100))
                except ValueError:
                    raise ValueError("Minimum Speed Cutoff must be from 0 to 1 km/h.") from None
            elif "hall_speed_min" in dirty:
                raise ValueError("Minimum Speed Cutoff must be a number from 0 to 1 km/h.")
        if _source_uses_parameter("speedo_max", sources):
            text = get("speedo_max")
            if text:
                try:
                    raw = int(text)
                except ValueError:
                    raise ValueError("Speedometer Max must be a number.") from None
                if not 1000 <= raw <= 30000:
                    raise ValueError(
                        "Speedometer Max must be between 10 and 300 mph (or the equivalent in km/h).")
            elif "speedo_max" in dirty:
                raise ValueError("Speedometer Max must be a number.")
    except ValueError as exc:
        fail("Invalid speed setting", exc)

    # Warning thresholds (_validate_warning_inputs); stored kPa and degC
    try:
        for param, check, unit in (("oil_warn_kpa", _oil_warning_to_raw, "kPa"),
                                   ("coolant_warn_c", _temperature_warning_to_raw, "C")):
            text = get(param)
            if not text:
                if param in dirty:
                    raise ValueError(f"{param} must be a number.")
                continue
            check(text, unit)
    except ValueError as exc:
        fail("Invalid warning threshold", exc)

    # Fuel capacity (_validate_fuel_capacity); stored in gallons
    text = get("fuel_capacity")
    if not text:
        if "fuel_capacity" in dirty:
            fail("Invalid fuel capacity", ValueError("Fuel tank capacity must be a number."))
    else:
        try:
            _fuel_capacity_to_raw(text, "gal")
        except ValueError as exc:
            fail("Invalid fuel capacity", exc)

    # Gauge ranges (_validate_motor_ranges)
    for title, (func_p, min_p, max_p) in zip(MOTOR_TITLES, MOTOR_PARAMS):
        if get(func_p) in ("", "0"):
            continue
        try:
            lo, hi = int(get(min_p)), int(get(max_p))
        except ValueError:
            fail("Invalid gauge range", ValueError(f"{title}: gauge min and max must be whole numbers."))
        if not (-32768 <= lo <= 32767 and -32768 <= hi <= 32767) or lo == hi:
            fail("Invalid gauge range", ValueError(
                f"{title}: gauge min and max must differ (both between -32768 and 32767)."))

    # Sensor curves (_validate_curves)
    try:
        updates, newly_dirty = compute_curves(values, dirty)
    except ValueError as exc:
        fail("Invalid sensor curve", exc)

    # Low side outputs (_validate_outputs)
    try:
        validate_outputs(values, imperial)
    except ValueError as exc:
        fail("Invalid output configuration", exc)

    # Warning delays (_validate_warning_delays); stored in ms
    for param in WARN_DEBOUNCE_PARAMS:
        text = get(param).strip()
        if not text:
            continue
        try:
            ms = int(text)
        except ValueError:
            fail("Invalid warning delay", ValueError(
                f"{WARN_DEBOUNCE_NAMES[param]}: Delay must be a number of seconds from 0 to "
                f"{WARN_DEBOUNCE_MAX_S}."))
        if not 0 <= ms <= WARN_DEBOUNCE_MAX_S * 1000:
            fail("Invalid warning delay", ValueError(
                f"{WARN_DEBOUNCE_NAMES[param]}: Delay must be from 0 to {WARN_DEBOUNCE_MAX_S} seconds."))
    return updates, newly_dirty


def params_to_write(values: Dict[str, str], dirty) -> list:
    """Edited parameters in ALL_PARAMS order (output mode last), skipping ones not in use."""
    sources = selected_sources(values)
    return [
        p for p in ALL_PARAMS
        if p in dirty and values.get(p, "")
        and _source_uses_parameter(p, sources)
        and output_param_active(p, values)
    ]


def migrate_loaded_config(params: Dict[str, str]) -> tuple:
    """Mirror of GaugeConfigApp._load_from_file once the file has been read.

    Returns ({param: stored value} for every known parameter in the file, tool unit system).
    """
    params = dict(params)
    if "tach_max" in params and "tach_flash" not in params:
        params["tach_flash"] = params["tach_max"]
    tach_keys = ("num_leds", "warn_leds", "shift_leds", "tach_min", "tach_max")
    if any(p in params for p in tach_keys):
        for param, default in TACH_COLORS.items():
            params.setdefault(param, default)
    if "tach_sweep" not in params and any(p in params for p in tach_keys):
        params["tach_sweep"] = "2"
    converted, units = _decode_config_units(params)
    converted.update(_decode_config_filters(params))
    system = params.get("tool_units")
    if system not in UNIT_SYSTEMS:
        system = "imperial" if units["speedo_max_units"] == "mph" else "metric"
    # Older formats convert warnings and capacity to floats; store them as gui.py would.
    for param in ("oil_warn_kpa", "coolant_warn_c", "fuel_capacity"):
        if param in converted:
            converted[param] = float_text(float(converted[param]))
    known = set(ALL_PARAMS)
    values = {
        k: converted.get(k, v) for k, v in params.items()
        if k not in LOAD_SKIP_KEYS and k in known
    }
    return values, system


def config_for_save(values: Dict[str, str], system: str) -> Dict[str, str]:
    """The parameter set GaugeConfigApp._save_to_file writes, including the format markers."""
    if system not in UNIT_SYSTEMS:
        system = "metric"
    params = {p: values[p] for p in ALL_PARAMS if values.get(p, "")}
    params["config_format"] = str(CONFIG_FILE_FORMAT)
    params["filter_format"] = "7"
    params.update(FIELD_UNITS[system])
    params["tool_units"] = system
    _decode_config_filters(params)
    return params
