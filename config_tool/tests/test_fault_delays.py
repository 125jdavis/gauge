"""Fault warning debounce delay (seconds in the tool, ms in firmware)."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from gui import (  # noqa: E402
    ALL_PARAMS,
    TOOLTIPS,
    WARN_DEBOUNCE_PARAMS,
    debounce_ms_to_seconds,
    debounce_seconds_to_ms,
)


def test_params_registered():
    assert set(WARN_DEBOUNCE_PARAMS) <= set(ALL_PARAMS)
    assert all(p in TOOLTIPS for p in WARN_DEBOUNCE_PARAMS)


@pytest.mark.parametrize("text,ms", [("3", 3000), ("0", 0), ("0.25", 250), ("60", 60000), (" 1.5 ", 1500)])
def test_seconds_to_ms(text, ms):
    assert debounce_seconds_to_ms(text) == ms
    assert debounce_seconds_to_ms(debounce_ms_to_seconds(str(ms))) == ms


@pytest.mark.parametrize("text", ["", "abc", "-1", "60.5", "inf", "nan"])
def test_seconds_rejected(text):
    with pytest.raises(ValueError):
        debounce_seconds_to_ms(text)


def test_ms_display():
    assert debounce_ms_to_seconds("3000") == "3"
    assert debounce_ms_to_seconds("250") == "0.25"
    assert debounce_ms_to_seconds("") == ""
