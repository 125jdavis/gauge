"""Splash image catalog: firmware, config tool and preview PNGs must agree."""
import os
import re
import sys

import pytest

HERE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(HERE, ".."))
import gui_model as m  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
FIRMWARE = os.path.join(REPO, "gauge_V4")
PREVIEWS = os.path.join(REPO, "config_tool", "web", "splash")


def _read(name):
    with open(os.path.join(FIRMWARE, name), encoding="utf-8") as f:
        return f.read()


def _catalog():
    """[(symbol, 512 bitmap bytes)] in SPLASH_IMAGES order."""
    src = _read("image_data.cpp")
    body = re.search(r"SPLASH_IMAGES\[\]\s*PROGMEM\s*=\s*\{(.*?)\};", src, re.S).group(1)
    out = []
    for sym in re.findall(r"^\s*(IMG_\w+),", body, re.M):
        arr = re.search(rf"\b{sym}\s*\[\]\s*PROGMEM\s*=\s*\{{(.*?)\}};", src, re.S).group(1)
        data = bytes(int(h, 16) for h in re.findall(r"0x([0-9a-fA-F]{2})", arr))
        assert len(data) >= 512, sym
        out.append((sym, data[:512]))  # drawBitmap reads 128x32 = 512 bytes
    return out


def _enum(name):
    return int(re.search(rf"\b{name}\s*=\s*(\d+)", _read("config_calibration.h")).group(1))


def test_tool_lists_every_firmware_splash_image():
    assert len(_catalog()) == len(m.SPLASH_BUILTIN_OPTIONS)


def test_custom_ids_and_defaults_match_firmware():
    assert m.SPLASH_CUSTOM_IDS == (_enum("SPLASH_CUSTOM_1"), _enum("SPLASH_CUSTOM_2"))
    assert m.SPLASH_DEFAULTS == {"splash1_img": _enum("SPLASH1_DEFAULT"),
                                 "splash2_img": _enum("SPLASH2_DEFAULT")}
    assert all(id_ >= len(m.SPLASH_BUILTIN_OPTIONS) for id_ in m.SPLASH_CUSTOM_IDS)


def test_preview_pngs_match_firmware_bitmaps():
    pytest.importorskip("PIL")
    from ssd1306_converter import SSD1306Converter
    from PIL import Image
    conv = SSD1306Converter()
    for i, (sym, data) in enumerate(_catalog()):
        with Image.open(os.path.join(PREVIEWS, f"{i}.png")) as img:
            assert conv.convert_pil(img) == data, f"web/splash/{i}.png does not match {sym}"


def test_converter_layout_matches_builtin_images():
    """Converting a preview of a firmware bitmap reproduces the bitmap (same layout as drawBitmap)."""
    pytest.importorskip("PIL")
    from ssd1306_converter import SSD1306Converter
    conv = SSD1306Converter()
    for sym, data in _catalog():
        assert conv.convert_pil(conv.preview(data)) == data, sym


@pytest.mark.parametrize("value, ok", [
    ("0", True), ("10", True), ("11", False), ("200", True), ("201", True),
    ("202", False), ("-1", False), ("x", False),
])
def test_splash_id_validation(value, ok):
    assert m.splash_id_valid(value) is ok


def test_validate_config_rejects_unknown_splash_image():
    with pytest.raises(m.ConfigInvalid) as exc:
        m.validate_config({"splash2_img": "50"}, {"splash2_img"})
    assert exc.value.title == "Invalid splash image"
    assert "Display 2" in str(exc.value)


def test_splash_params_are_written_like_other_settings():
    values = {"splash1_img": "6", "splash2_img": "200"}
    assert m.params_to_write(values, set(values)) == ["splash1_img", "splash2_img"]
