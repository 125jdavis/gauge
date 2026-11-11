import os
import sys
import tkinter as tk
from types import SimpleNamespace

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from gui import GaugeConfigApp, TACH_COLORS, tach_color, tach_display_color, tach_hsv_color, tach_led_step, tach_step_rpm, tach_steps


def use_color_dialog(app: GaugeConfigApp, hue: int, brightness: int, action: str) -> None:
    def interact() -> None:
        dialog = next(w for w in app.root.winfo_children() if isinstance(w, tk.Toplevel))
        sliders = [w for w in dialog.winfo_children() if isinstance(w, tk.Scale)]
        sliders[0].set(hue)
        sliders[1].set(brightness)
        dialog.update()
        button = next(w for w in dialog.winfo_children()
                      if isinstance(w, tk.ttk.Button) and w.cget("text") == action)
        button.invoke()
    app.root.after(50, interact)
    app._choose_tach_color("tach_color_normal")


@pytest.mark.parametrize("count", [1, 2, 5, 6, 26, 27, 64])
@pytest.mark.parametrize("sweep", range(4))
def test_tach_geometry_and_integer_thresholds(count: int, sweep: int) -> None:
    steps = tach_steps(count, sweep)
    positions = [tach_led_step(i, count, sweep) for i in range(count)]
    assert set(positions) == set(range(steps))
    if sweep >= 2:
        assert positions == positions[::-1]
    for step in range(steps):
        rpm = tach_step_rpm(step, steps, 3000, 6000)
        if steps > 1:
            assert (rpm - 3000) * (steps - 1) // 3000 == step
            if step:
                assert (rpm - 1 - 3000) * (steps - 1) // 3000 == step - 1


def test_tach_editor_drag_and_fit() -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    try:
        for param, value in {
            "num_leds": "7", "tach_min": "3000", "tach_max": "6000",
            "tach_sweep": "2", "warn_leds": "2", "shift_leds": "1",
        }.items():
            app._vars[param].set(value)
        app.root.update_idletasks()
        texts = [app._tach_canvas.itemcget(item, "text")
                 for item in app._tach_canvas.find_all()
                 if app._tach_canvas.type(item) == "text"]
        assert texts.count("Min 3000") == 2
        assert "Max 6000" in texts
        width = max(app._tach_canvas.winfo_width(), 100)
        positions = app._tach_positions(1, 7, 2, width)
        assert len(positions) == 2
        app._tach_drag(SimpleNamespace(x=positions[1], y=86))
        assert app._vars["warn_leds"].get() == "2"
        app._tach_drag_param = None
        app._tach_drag(SimpleNamespace(x=positions[0], y=143))
        assert app._vars["shift_leds"].get() == "2"
        assert app._dirty["shift_leds"]
        app._vars["num_leds"].set("2")
        app._normalize_tach_zones()
        assert app._tach_values()[-2:] == (0, 0)
    finally:
        app.root.destroy()


def test_color_and_live_preview_lifecycle(monkeypatch: pytest.MonkeyPatch) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")

    class FakeSerial:
        is_connected = True

        def __init__(self) -> None:
            self.previews: list[str] = []
            self.stops = 0

        def tach_preview(self, payload: str) -> None:
            self.previews.append(payload)

        def tach_preview_stop(self) -> None:
            self.stops += 1

    serial = FakeSerial()
    app._serial = serial
    try:
        app._notebook.select(5)
        for param, value in {
            "num_leds": "7", "tach_min": "3000", "tach_max": "6000",
            "tach_sweep": "2", "warn_leds": "2", "shift_leds": "1",
        }.items():
            app._vars[param].set(value)
        use_color_dialog(app, 210, 40, "Apply")
        app.root.update_idletasks()
        assert app._dirty["tach_color_normal"]
        rectangles = [app._tach_canvas.itemcget(item, "fill")
                      for item in app._tach_canvas.find_all()
                      if app._tach_canvas.type(item) == "rectangle"]
        assert tach_display_color("003366") in rectangles
        app._toggle_tach_test()
        assert serial.previews[-1] == "0720201003366500A00500000"
        app._vars["tach_color_warning"].set("ABCDEF")
        app.root.after_cancel(app._tach_test_after)
        app._refresh_tach_test()
        assert serial.previews[-1] == "0720201003366ABCDEF500000"
        app._notebook.select(0)
        app._on_tab_changed()
        assert serial.stops == 1
        assert not app._tach_test_on
        assert app._tach_test_after is None
    finally:
        app._stop_tach_test()
        app.root.destroy()


def test_preview_toggle_and_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")

    class FakeSerial:
        is_connected = True
        stops = 0
        fail = False

        def tach_preview(self, payload: str) -> None:
            if self.fail:
                raise OSError("disconnected")

        def tach_preview_stop(self) -> None:
            self.stops += 1

    errors = []
    monkeypatch.setattr("gui.messagebox.showerror", lambda title, message: errors.append(message))
    serial = FakeSerial()
    app._serial = serial
    try:
        for param, value in {
            "num_leds": "26", "tach_min": "3000", "tach_max": "6000",
            "tach_sweep": "2", "warn_leds": "6", "shift_leds": "2",
        }.items():
            app._vars[param].set(value)
        app._toggle_tach_test()
        app._toggle_tach_test()
        assert serial.stops == 1
        assert not app._tach_test_on
        app._toggle_tach_test()
        app.root.after_cancel(app._tach_test_after)
        serial.fail = True
        app._refresh_tach_test()
        assert not app._tach_test_on
        assert "expires automatically" in errors[-1]
    finally:
        app._stop_tach_test()
        app.root.destroy()


def test_legacy_config_migrates_flash_and_colors(tmp_path, monkeypatch: pytest.MonkeyPatch) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    path = tmp_path / "old.txt"
    path.write_text("tach_max=7200\ntach_min=3000\n", encoding="utf-8")
    monkeypatch.setattr("gui.filedialog.askopenfilename", lambda **_kwargs: str(path))
    try:
        app._load_from_file()
        assert app._vars["tach_flash"].get() == "7200"
        assert app._dirty["tach_flash"]
        assert {p: app._vars[p].get() for p in TACH_COLORS} == TACH_COLORS
    finally:
        app.root.destroy()


def test_flash_validation_allows_independent_threshold(monkeypatch: pytest.MonkeyPatch) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    errors = []
    monkeypatch.setattr("gui.messagebox.showerror", lambda title, message: errors.append(message))
    try:
        for param, value in {
            "num_leds": "26", "tach_min": "3000", "tach_max": "6000",
            "tach_sweep": "2", "warn_leds": "6", "shift_leds": "2",
        }.items():
            app._vars[param].set(value)
        for value in ("0", "5000", "6000", "7200", "15000"):
            app._vars["tach_flash"].set(value)
            assert app._validate_tach()
        for value in ("", "-1", "15001", "oops"):
            app._vars["tach_flash"].set(value)
            assert not app._validate_tach()
        assert errors
    finally:
        app.root.destroy()


@pytest.mark.parametrize("value", ["", "#123456", "12345", "12345Z", "1234567"])
def test_invalid_tach_color(value: str) -> None:
    with pytest.raises(ValueError):
        tach_color(value)


@pytest.mark.parametrize(("value", "expected"), [
    ("1E0F00", "#A55200"),
    ("500A00", "#B91700"),
    ("500000", "#B90000"),
    ("000000", "#990000"),
    ("800000", "#CC0000"),
    ("123456", "#2771BB"),
    ("FFFFFF", "#FFFFFF"),
])
def test_tach_preview_brightens_without_swapping_channels(value: str, expected: str) -> None:
    assert tach_display_color(value) == expected


def test_loaded_colors_change_preview_not_stored_values(monkeypatch: pytest.MonkeyPatch) -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    try:
        for param, value in {
            "num_leds": "26", "tach_min": "3000", "tach_max": "6000",
            "tach_sweep": "2", "warn_leds": "6", "shift_leds": "2",
            **TACH_COLORS,
        }.items():
            app._vars[param].set(value)
        app.root.update_idletasks()
        for param, value in TACH_COLORS.items():
            assert app._tach_color_swatches[param].cget("background") == tach_display_color(value)
            assert app._vars[param].get() == value
        rectangles = {app._tach_canvas.itemcget(item, "fill")
                      for item in app._tach_canvas.find_all()
                      if app._tach_canvas.type(item) == "rectangle"}
        assert rectangles == {tach_display_color(value) for value in TACH_COLORS.values()}
        assert app._tach_preview_payload().endswith("1E0F00500A00500000")
        use_color_dialog(app, 120, 100, "Cancel")
        assert app._vars["tach_color_normal"].get() == "1E0F00"
        use_color_dialog(app, 120, 0, "Apply")
        assert app._vars["tach_color_normal"].get() == "000000"
        assert app._tach_color_swatches["tach_color_normal"].cget("background") == "#009900"
        use_color_dialog(app, 120, 100, "Apply")
        assert app._vars["tach_color_normal"].get() == "00FF00"
    finally:
        app.root.destroy()


def test_color_dialog_apply_without_edits_preserves_loaded_rgb() -> None:
    try:
        app = GaugeConfigApp()
    except tk.TclError as exc:
        pytest.skip(f"Tk display unavailable: {exc}")
    try:
        app._vars["tach_color_normal"].set("123456")
        app._dirty["tach_color_normal"] = False

        def apply() -> None:
            dialog = next(w for w in app.root.winfo_children() if isinstance(w, tk.Toplevel))
            button = next(w for w in dialog.winfo_children()
                          if isinstance(w, tk.ttk.Button) and w.cget("text") == "Apply")
            button.invoke()

        app.root.after(50, apply)
        app._choose_tach_color("tach_color_normal")
        assert app._vars["tach_color_normal"].get() == "123456"
        assert not app._dirty["tach_color_normal"]
    finally:
        app.root.destroy()


@pytest.mark.parametrize(("hue", "brightness", "expected"), [
    (0, 0, "000000"), (0, 100, "FF0000"), (120, 100, "00FF00"),
    (240, 100, "0000FF"), (60, 50, "808000"),
])
def test_tach_hsv_sliders(hue: int, brightness: int, expected: str) -> None:
    assert tach_hsv_color(hue, brightness) == expected


def test_tach_display_mapping_all_brightness_values() -> None:
    for level in range(101):
        stored = tach_hsv_color(0, level)
        display_red = int(tach_display_color(stored)[1:3], 16)
        stored_red = int(stored[:2], 16)
        assert abs(display_red - (153 + 0.4 * stored_red)) <= 0.5
