"""Low side output (D12/D13) validation and PWM curve mirror tests."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from gui import (  # noqa: E402
    ALL_PARAMS,
    MOTOR_FUNCTION_OPTIONS,
    OUTPUT_PARAM_OPTIONS,
    OUTPUT_PARAMS,
    output_off_operator,
    output_param_active,
    output_param_names,
    pwm_duty_permille,
    validate_on_off,
    validate_outputs,
    validate_pwm_curve,
)

XS = ["60", "70", "75", "80", "85", "90", "95", "100"]
DS = ["0", "0", "20", "35", "50", "65", "80", "100"]


def pwm_values(n=1, freq="100", xs=XS, ds=DS):
    p = f"out{n}_"
    values = {p + "mode": "2", p + "param": "2", p + "freq": freq}
    values.update({f"{p}x{i}": x for i, x in enumerate(xs)})
    values.update({f"{p}d{i}": d for i, d in enumerate(ds)})
    return values


def test_params_registered_and_mode_written_last():
    assert all(p in ALL_PARAMS for p in OUTPUT_PARAMS)
    assert output_param_names(1)[-1] == "out1_mode"
    assert len(OUTPUT_PARAMS) == 2 * 22
    # Firmware values 1-9 match MOTOR_FUNC_*, 10 is battery voltage
    assert OUTPUT_PARAM_OPTIONS[:9] == MOTOR_FUNCTION_OPTIONS[1:]
    assert len(OUTPUT_PARAM_OPTIONS) == 10


def test_off_operator_is_opposite():
    assert output_off_operator(0) == "<"
    assert output_off_operator(1) == ">"


@pytest.mark.parametrize("op,on,off", [("0", "95", "90"), ("1", "115", "125")])
def test_on_off_logical(op, on, off):
    validate_on_off(op, on, off)


@pytest.mark.parametrize("op,on,off", [("0", "95", "95"), ("0", "95", "100"), ("1", "115", "110"),
                                       ("0", "95", ""), ("0", "40000", "0")])
def test_on_off_rejected(op, on, off):
    with pytest.raises(ValueError):
        validate_on_off(op, on, off)


def test_pwm_curve_valid():
    validate_pwm_curve("20", XS, DS)
    validate_pwm_curve("500", XS, list(reversed(DS)))  # Duty may fall as the value rises


@pytest.mark.parametrize("freq,xs,ds", [
    ("19", XS, DS), ("501", XS, DS),
    ("100", ["60", "70", "70", "80", "85", "90", "95", "100"], DS),   # Not strictly ascending
    ("100", ["60", "50", "75", "80", "85", "90", "95", "100"], DS),   # Descending
    ("100", XS, ["0", "0", "20", "35", "50", "65", "80", "101"]),     # Duty over 100 %
    ("100", XS, ["0", "", "20", "35", "50", "65", "80", "100"]),      # Missing cell
])
def test_pwm_curve_rejected(freq, xs, ds):
    with pytest.raises(ValueError):
        validate_pwm_curve(freq, xs, ds)


def test_validate_outputs_skips_unread_and_disabled():
    validate_outputs({})
    validate_outputs({"out1_mode": "0", "out1_on_val": "junk"})


def test_shared_pwm_frequency_enforced():
    validate_outputs({**pwm_values(1, "100"), **pwm_values(2, "100")})
    with pytest.raises(ValueError, match="same frequency"):
        validate_outputs({**pwm_values(1, "100"), **pwm_values(2, "200")})
    # A non-PWM output does not constrain the frequency
    validate_outputs({**pwm_values(1, "100"), "out2_mode": "1", "out2_param": "3",
                      "out2_on_op": "0", "out2_on_val": "3000", "out2_off_val": "2800"})


def test_error_names_output():
    with pytest.raises(ValueError, match="Output 2"):
        validate_outputs({"out2_mode": "1", "out2_param": "2", "out2_on_op": "1",
                          "out2_on_val": "50", "out2_off_val": "40"})


def test_inactive_mode_settings_not_written():
    values = {"out1_mode": "1"}
    assert output_param_active("out1_on_val", values)
    assert not output_param_active("out1_freq", values)
    assert not output_param_active("out1_x3", values)
    assert output_param_active("out1_param", values)
    assert output_param_active("m1_func", values)


def test_duty_mirror_matches_firmware_expectations():
    xs = [int(x) for x in XS]
    ds = [int(d) for d in DS]
    # Same cases as gauge_V4/tests/output_logic_test.cpp
    assert pwm_duty_permille(xs, ds, -40) == 0
    assert pwm_duty_permille(xs, ds, 200) == 1000
    assert pwm_duty_permille(xs, ds, 75) == 200
    assert pwm_duty_permille(xs, ds, 77) == 260
    assert pwm_duty_permille(xs, ds, 99) == 960
    xw = [-32768, -1000, 0, 1, 2, 3, 4, 32767]
    dw = [100, 90, 80, 70, 60, 50, 40, 0]
    assert pwm_duty_permille(xw, dw, 16383) == 201


# ---- Tool-wide unit system (display only; stored values stay metric) ----
from gui import (  # noqa: E402
    av_physical_unit,
    display_to_raw,
    physical_to_raw,
    raw_to_display,
    raw_to_physical,
    unit_label,
)


@pytest.mark.parametrize("kind,raw,shown", [
    ("temp", "95", "203"), ("temp", "-40", "-40"), ("speed", "100", "62.1"),
    ("pressure", "300", "43.5"), ("pressure_abs", "101", "14.6"),
])
def test_imperial_display_round_trips(kind, raw, shown):
    assert raw_to_display(raw, kind, True) == shown
    assert display_to_raw(shown, kind, True) == int(raw)


def test_metric_and_unitless_pass_through():
    assert raw_to_display("95", "temp", False) == "95"
    assert raw_to_display("3000", None, True) == "3000"
    assert display_to_raw("3000", None, True) == 3000
    assert raw_to_display("junk", "temp", True) == "junk"
    with pytest.raises(ValueError):
        display_to_raw("9.5", "temp", False)  # Metric values are whole numbers
    with pytest.raises(ValueError):
        display_to_raw("abc", "temp", True)


def test_unit_labels():
    assert unit_label("temp", True) == "°F"
    assert unit_label("speed", False) == "km/h"
    assert unit_label(None, True, "RPM") == "RPM"


def test_validation_messages_use_selected_units():
    values = {"out1_mode": "1", "out1_param": "2", "out1_on_op": "0",
              "out1_on_val": "95", "out1_off_val": "96"}
    with pytest.raises(ValueError, match="203 °F"):
        validate_outputs(values, imperial=True)
    with pytest.raises(ValueError, match="95 °C"):
        validate_outputs(values, imperial=False)


def test_av_pressure_in_psi():
    assert av_physical_unit(0, imperial=True) == "PSI (abs)"
    assert av_physical_unit(5, imperial=True) == "Lambda"
    raw = physical_to_raw("14.7", "PSI (abs)")  # 1 atm, stored as kPa * 10
    assert abs(raw - 1014) <= 1
    assert abs(raw_to_physical(raw, "PSI (abs)") - 14.7) < 0.02
