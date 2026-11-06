import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from gui import (
    CAN_PROTOCOL_OPTIONS,
    MOTOR_FUNCTION_OPTIONS,
    MOTOR_FUNCTION_PARAMS,
    MOTOR_FUNCTION_UNITS,
    MOTOR_FUNCTION_RANGES,
    MOTOR_PARAMS,
    DIRECTION_OPTIONS,
    _gauge_range_to_device,
    _gauge_range_from_device,
    ALL_PARAMS,
    FUEL_CAPACITY_UNITS,
    OIL_WARNING_UNITS,
    SPEEDO_MAX_UNITS,
    TEMPERATURE_UNITS,
    SOURCE_DEPENDENT_PARAMS,
    _hall_speed_min_to_raw,
    _source_uses_parameter,
    _oil_warning_to_raw,
    _speedo_max_to_raw,
    _temperature_warning_to_raw,
    _warning_display_value,
    _fuel_capacity_to_raw,
    _fuel_capacity_display_value,
    _decode_config_units,
)


def test_unit_radio_choices_put_imperial_first() -> None:
    assert SPEEDO_MAX_UNITS == ("mph", "km/h")
    assert OIL_WARNING_UNITS == ("PSI", "kPa")
    assert TEMPERATURE_UNITS == ("F", "C")
    assert FUEL_CAPACITY_UNITS == ("gal", "L")


def test_can_protocol_dropdown_order_matches_firmware_protocol_values() -> None:
    assert CAN_PROTOCOL_OPTIONS == ("Haltech v2", "Megasquirt", "AiM", "OBDII")


def test_motor_function_dropdown_matches_firmware_values() -> None:
    assert MOTOR_FUNCTION_OPTIONS == ("Off", "Fuel level", "Coolant temp")
    assert all(p in ALL_PARAMS for p in MOTOR_FUNCTION_PARAMS)


def test_motor_range_params_are_configurable() -> None:
    for params in MOTOR_PARAMS:
        assert all(p in ALL_PARAMS for p in params)
    assert DIRECTION_OPTIONS == ("CW", "CCW")
    assert len(MOTOR_FUNCTION_UNITS) == len(MOTOR_FUNCTION_RANGES) == len(MOTOR_FUNCTION_OPTIONS)


def test_ccw_swaps_min_and_max_in_config() -> None:
    assert _gauge_range_to_device("10", "100", ccw=False) == ("10", "100")
    assert _gauge_range_to_device("10", "100", ccw=True) == ("100", "10")
    assert _gauge_range_from_device("10", "100") == ("10", "100", False)
    assert _gauge_range_from_device("100", "10") == ("10", "100", True)
    assert _gauge_range_from_device("", "") == ("", "", False)


@pytest.mark.parametrize(
    ("value", "expected"),
    [("0", 0), ("0.5", 50), ("1.00", 100)],
)
def test_hall_speed_min_uses_kilometers_per_hour(value: str, expected: int) -> None:
    assert _hall_speed_min_to_raw(value) == expected


@pytest.mark.parametrize("value", ["-0.01", "1.01", "invalid", "nan", "inf"])
def test_hall_speed_min_rejects_out_of_range_values(value: str) -> None:
    with pytest.raises(ValueError):
        _hall_speed_min_to_raw(value)


@pytest.mark.parametrize(
    ("value", "units", "expected"),
    [("100", "mph", 10000), ("160.9344", "km/h", 10000), ("100", "km/h", 6214)],
)
def test_speedo_max_converts_display_units_to_firmware_value(
    value: str, units: str, expected: int,
) -> None:
    assert _speedo_max_to_raw(value, units) == expected


def test_speedo_max_rejects_invalid_values() -> None:
    for value, units in [("0", "mph"), ("301", "mph"), ("invalid", "km/h"), ("100", "kph")]:
        with pytest.raises(ValueError):
            _speedo_max_to_raw(value, units)


def test_source_specific_speed_and_rpm_settings() -> None:
    sources = {"speed_source": "1", "rpm_source": "1"}
    for param in ("revs_per_km", "teeth_per_rev", "filter_hall", "hall_speed_min"):
        assert not _source_uses_parameter(param, sources)
    for param in ("cyl_count", "filter_rpm", "rpm_debounce_us", "engine_rpm_min"):
        assert not _source_uses_parameter(param, sources)

    sources.update(speed_source="2", rpm_source="2")
    for param in ("revs_per_km", "teeth_per_rev", "filter_hall", "hall_speed_min"):
        assert _source_uses_parameter(param, sources)
    for param in ("cyl_count", "filter_rpm", "rpm_debounce_us", "engine_rpm_min"):
        assert _source_uses_parameter(param, sources)


def test_sensor_filters_are_enabled_if_any_selected_source_uses_input() -> None:
    sources = {"coolant_src": "1", "oil_temp_src": "2"}
    assert _source_uses_parameter("filter_therm", sources)
    sources["oil_temp_src"] = "1"
    assert not _source_uses_parameter("filter_therm", sources)

    sources.update(oil_prs_source="1", map_source="2")
    assert _source_uses_parameter("filter_av1", sources)
    sources["map_source"] = "1"
    assert not _source_uses_parameter("filter_av1", sources)


def test_can_protocol_stays_editable_and_speedometer_requires_speed() -> None:
    sources = {"speed_source": "4", "rpm_source": "3"}
    assert _source_uses_parameter("can_protocol", sources)
    assert _source_uses_parameter("speedo_max", sources)
    assert not _source_uses_parameter("speedo_max", {"speed_source": "0"})
    sources["oil_prs_source"] = "1"
    assert _source_uses_parameter("can_protocol", sources)


def test_all_source_dependent_parameters_have_source_rules() -> None:
    assert set(SOURCE_DEPENDENT_PARAMS) == {
        "revs_per_km", "teeth_per_rev", "filter_hall", "hall_speed_min",
        "cyl_count", "filter_rpm", "rpm_debounce_us", "engine_rpm_min",
        "filter_fuel", "filter_therm", "filter_av1", "filter_av2", "filter_av3",
    }


@pytest.mark.parametrize(
    ("value", "units", "expected"),
    [("60", "kPa", 60), ("8.702264", "PSI", 60)],
)
def test_oil_warning_threshold_converts_to_firmware_kpa(
    value: str, units: str, expected: float,
) -> None:
    assert _oil_warning_to_raw(value, units) == pytest.approx(expected)


@pytest.mark.parametrize(
    ("value", "units", "expected"),
    [("110", "C", 110), ("230", "F", 110)],
)
def test_temperature_warning_threshold_converts_to_firmware_celsius(
    value: str, units: str, expected: float,
) -> None:
    assert _temperature_warning_to_raw(value, units) == pytest.approx(expected)


@pytest.mark.parametrize(
    ("param", "raw", "units", "expected"),
    [
        ("oil_warn_kpa", "60", "PSI", "8.7"),
        ("coolant_warn_c", "110", "F", "230"),
    ],
)
def test_warning_threshold_display_converts_from_firmware_units(
    param: str, raw: str, units: str, expected: str,
) -> None:
    assert _warning_display_value(raw, param, units) == expected


def test_warning_threshold_converters_enforce_firmware_ranges() -> None:
    for value, units in [("-1", "kPa"), ("1001", "kPa"), ("nan", "PSI")]:
        with pytest.raises(ValueError):
            _oil_warning_to_raw(value, units)
    for value, units in [("49", "C"), ("201", "C"), ("393", "F"), ("inf", "F")]:
        with pytest.raises(ValueError):
            _temperature_warning_to_raw(value, units)


@pytest.mark.parametrize(
    ("value", "units", "expected"),
    [("16", "gal", 16), ("60.566588544", "L", 16)],
)
def test_fuel_capacity_converts_to_firmware_gallons(
    value: str, units: str, expected: float,
) -> None:
    assert _fuel_capacity_to_raw(value, units) == pytest.approx(expected)


@pytest.mark.parametrize(
    ("raw", "units", "expected"),
    [("16", "gal", "16"), ("16", "L", "60.57")],
)
def test_fuel_capacity_display_conversion(
    raw: str, units: str, expected: str,
) -> None:
    assert _fuel_capacity_display_value(raw, units) == expected


def test_fuel_capacity_conversion_enforces_firmware_range() -> None:
    for value, units in [("0.9", "gal"), ("757.1", "L"), ("nan", "gal")]:
        with pytest.raises(ValueError):
            _fuel_capacity_to_raw(value, units)


def test_format_5_keeps_firmware_units_and_only_restores_display_preferences() -> None:
    params = {
        "config_format": "5",
        "hall_speed_min": "50",
        "speedo_max": "10000",
        "oil_warn_kpa": "60",
        "coolant_warn_c": "110",
        "fuel_capacity": "16",
        "speedo_max_units": "km/h",
        "oil_warn_units": "PSI",
        "coolant_warn_units": "F",
        "fuel_capacity_units": "L",
    }

    converted, units = _decode_config_units(params)

    assert converted == {}
    assert units == {
        "speedo_max_units": "km/h",
        "oil_warn_units": "PSI",
        "coolant_warn_units": "F",
        "fuel_capacity_units": "L",
    }


@pytest.mark.parametrize(
    ("version", "params", "expected"),
    [
        ("2", {"hall_speed_min": "0.5", "speedo_max": "100"}, {"hall_speed_min": "50", "speedo_max": "10000"}),
        ("3", {"oil_warn_kpa": "8.7", "coolant_warn_c": "230", "oil_warn_units": "PSI", "coolant_warn_units": "F"}, {"oil_warn_kpa": "59.9843884630586", "coolant_warn_c": "110.0"}),
        ("4", {"fuel_capacity": "60.566588544", "fuel_capacity_units": "L"}, {"fuel_capacity": "16.0"}),
    ],
)
def test_legacy_config_formats_are_converted_to_firmware_units(
    version: str, params: dict[str, str], expected: dict[str, str],
) -> None:
    converted, _units = _decode_config_units({"config_format": version, **params})
    for key, value in expected.items():
        assert float(converted[key]) == pytest.approx(float(value))
