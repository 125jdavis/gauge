"""Whole-configuration helpers in gui_model and the pywebview bridge (api.Api)."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import gui_model as m  # noqa: E402

BASE = {
    "speed_source": "2", "rpm_source": "2",
    "num_leds": "26", "tach_min": "3000", "tach_max": "6000", "tach_flash": "6200",
    "tach_sweep": "2", "warn_leds": "6", "shift_leds": "2", **m.TACH_COLORS,
    "therm_bias": "2490",
    **{f"therm_t{i}": str(t) for i, (t, _r) in enumerate(m.THERM_PRESETS[0])},
    **{f"therm_r{i}": str(r) for i, (_t, r) in enumerate(m.THERM_PRESETS[0])},
}
# Stored voltages that match the curve above, as a device that has saved it would report
BASE.update({f"therm_v{i}": str(mv) for i, (_t, _r, mv) in enumerate(m.thermistor_curve(m.THERM_PRESETS[0], 2490))})


def test_validate_config_accepts_a_good_config_and_computes_curve_voltages():
    values = {k: v for k, v in BASE.items() if k not in m.THERM_VOLT_PARAMS}
    updates, newly_dirty = m.validate_config(values, {"therm_bias"})
    assert set(newly_dirty) >= set(m.THERM_VOLT_PARAMS)
    assert updates["therm_v0"] == str(m.divider_millivolts(48, 2490))


@pytest.mark.parametrize("change, title", [
    ({"tach_flash": "20000"}, "Invalid LED tach settings"),
    ({"m1_sweep": "50"}, "Invalid sweep angle"),
    ({"hall_speed_min": "150"}, "Invalid speed setting"),
    ({"speedo_max": "500"}, "Invalid speed setting"),
    ({"oil_warn_kpa": "2000"}, "Invalid warning threshold"),
    ({"fuel_capacity": "500"}, "Invalid fuel capacity"),
    ({"m1_func": "3", "m1_min": "10", "m1_max": "10"}, "Invalid gauge range"),
    ({"therm_r1": "48"}, "Invalid sensor curve"),
    ({"out1_mode": "1", "out1_param": "1", "out1_on_op": "0", "out1_on_val": "10", "out1_off_val": "20"},
     "Invalid output configuration"),
    ({"oil_warn_ms": "90000"}, "Invalid warning delay"),
])
def test_validate_config_reports_each_check(change, title):
    with pytest.raises(m.ConfigInvalid) as exc:
        m.validate_config({**BASE, **change}, set(change))
    assert exc.value.title == title


def test_params_to_write_keeps_order_and_skips_unused_settings():
    values = {**BASE, "out1_mode": "2", "out1_on_val": "5", "out1_freq": "100",
              "revs_per_km": "1625", "filter_gps": "64"}
    dirty = {"out1_mode", "out1_on_val", "out1_freq", "revs_per_km", "filter_gps", "num_leds"}
    order = m.params_to_write(values, dirty)
    # ALL_PARAMS order with the output mode last; the GPS filter is unused and on_val inactive in PWM mode
    assert order == ["revs_per_km", "num_leds", "out1_freq", "out1_mode"]


def test_migrate_loaded_config_converts_old_formats():
    values, system = m.migrate_loaded_config({
        "config_format": "3", "speedo_max": "120", "speedo_max_units": "mph",
        "oil_warn_kpa": "10", "oil_warn_units": "PSI", "coolant_warn_c": "100", "coolant_warn_units": "C",
        "tach_max": "6000", "unknown_key": "1",
    })
    assert system == "imperial"
    assert values["speedo_max"] == "12000"
    assert values["oil_warn_kpa"] == "68.95"
    assert values["tach_flash"] == "6000" and values["tach_sweep"] == "2"
    assert "unknown_key" not in values and "config_format" not in values


def test_config_for_save_round_trips_through_migration():
    params = m.config_for_save({**BASE, "speedo_max": "12427"}, "metric")
    assert params["config_format"] == str(m.CONFIG_FILE_FORMAT)
    assert params["speedo_max_units"] == "km/h" and params["tool_units"] == "metric"
    values, system = m.migrate_loaded_config(params)
    assert system == "metric" and values["speedo_max"] == "12427"


def test_tach_preview_payload():
    assert m.tach_preview_payload(BASE) == "1A20602" + "".join(m.TACH_COLORS.values())


# ---- api.Api with a fake serial device ------------------------------------------------

class FakeSerial:
    def __init__(self, params):
        self.params = dict(params)
        self.log = []
        self.is_connected = True

    def dump(self):
        return dict(self.params)

    def get(self, p):
        return self.params[p]

    def set(self, p, v):
        self.log.append(("set", p, v))
        self.params[p] = v

    def save(self):
        self.log.append(("save",))

    def test_rpm(self, rpm):
        self.log.append(("rpm", rpm))

    def disconnect(self):
        self.is_connected = False


@pytest.fixture
def bridge():
    pytest.importorskip("webview")
    import api
    instance = api.Api()
    instance._serial = FakeSerial({**BASE, "filter_format": "7", "rpm_source": "1"})
    instance._emit = lambda *a, **k: None
    return instance


def test_write_device_writes_dirty_params_in_order_then_saves(bridge):
    values = {**bridge._serial.params, "revs_per_km": "1700", "num_leds": "30", "warn_leds": "6"}
    res = bridge.write_device(values, ["revs_per_km", "num_leds"], False)
    assert res["ok"], res
    assert res["written"] == ["revs_per_km", "num_leds"]
    assert bridge._serial.log == [("set", "revs_per_km", "1700"), ("set", "num_leds", "30"), ("save",)]


def test_write_device_returns_validation_errors_without_writing(bridge):
    res = bridge.write_device({**bridge._serial.params, "tach_flash": "x"}, ["tach_flash"], False)
    assert res == {"ok": False, "title": "Invalid LED tach settings",
                   "error": "Flash RPM must be an integer from 0 to 15000."}
    assert bridge._serial.log == []


def test_test_signal_switches_source_in_ram_and_restores_it(bridge):
    res = bridge.test_signal("rpm", 3000.4)
    assert res["ok"] and res["sent"]
    assert ("set", "rpm_source", "4") in bridge._serial.log and ("rpm", 3000) in bridge._serial.log
    restored = bridge.restore_sources()
    assert restored["ok"] and bridge._serial.params["rpm_source"] == "1"
    assert ("save",) not in bridge._serial.log


def test_read_device_rejects_old_filter_format(bridge):
    bridge._serial.params["filter_format"] = "6"
    res = bridge.read_device()
    assert not res["ok"] and "older filter format" in res["error"]
