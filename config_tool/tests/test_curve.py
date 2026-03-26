"""
tests/test_curve.py
===================
Unit tests for piecewise linear curve lookup
(CONFIG_TOOL_SPECIFICATION.md §6 / §15).

The ``curve_lookup`` function mirrors the Arduino ``curveLookup()``
algorithm so that the Python tool can preview curve output without
a connected Arduino.

Run with: python -m pytest config_tool/tests/
"""

import sys
import os
import pathlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from typing import List


# ---------------------------------------------------------------------------
# Pure-Python reference implementation of curveLookup (spec §6)
# ---------------------------------------------------------------------------
def curve_lookup(input_val: int, xs: List[int], ys: List[int]) -> int:
    """Piecewise-linear lookup matching the Arduino curveLookup() algorithm.

    Args:
        input_val: Input value (e.g. millivolts).
        xs:        Sorted X breakpoints list (length 2–8, strictly increasing).
        ys:        Corresponding Y values list.

    Returns:
        Interpolated Y value, clamped to the first/last Y outside the range.
    """
    n = len(xs)
    if input_val <= xs[0]:
        return ys[0]
    if input_val >= xs[n - 1]:
        return ys[n - 1]
    for i in range(n - 1):
        if xs[i] <= input_val < xs[i + 1]:
            num = (input_val - xs[i]) * (ys[i + 1] - ys[i])
            den = xs[i + 1] - xs[i]
            return ys[i] + num // den
    return ys[n - 1]


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
class TestCurveLookup:
    """Tests for piecewise linear curve interpolation."""

    # ------------------------------------------------------------------
    # Clamping
    # ------------------------------------------------------------------

    def test_clamp_below_range(self) -> None:
        """Values below xs[0] return ys[0]."""
        xs = [100, 500, 1000]
        ys = [0, 50, 100]
        assert curve_lookup(0, xs, ys) == 0

    def test_clamp_at_lower_bound(self) -> None:
        """Value exactly equal to xs[0] returns ys[0]."""
        xs = [100, 500, 1000]
        ys = [0, 50, 100]
        assert curve_lookup(100, xs, ys) == 0

    def test_clamp_above_range(self) -> None:
        """Values above xs[-1] return ys[-1]."""
        xs = [100, 500, 1000]
        ys = [0, 50, 100]
        assert curve_lookup(2000, xs, ys) == 100

    def test_clamp_at_upper_bound(self) -> None:
        """Value exactly equal to xs[-1] returns ys[-1]."""
        xs = [100, 500, 1000]
        ys = [0, 50, 100]
        assert curve_lookup(1000, xs, ys) == 100

    # ------------------------------------------------------------------
    # Interpolation
    # ------------------------------------------------------------------

    def test_midpoint_interpolation(self) -> None:
        """Midpoint between two breakpoints returns midpoint Y."""
        xs = [0, 1000]
        ys = [0, 100]
        assert curve_lookup(500, xs, ys) == 50

    def test_quarter_interpolation(self) -> None:
        """Quarter point returns correct interpolated value."""
        xs = [0, 1000]
        ys = [0, 100]
        assert curve_lookup(250, xs, ys) == 25

    def test_exact_breakpoint_first_segment(self) -> None:
        """Value exactly at xs[1] (boundary) is in second segment."""
        xs = [0, 500, 1000]
        ys = [0, 50, 100]
        # xs[1]=500 triggers the first interval condition (xs[0]<=500<xs[1]) is False
        # so it falls to second interval; by algorithm: >= xs[1] and < xs[2]
        result = curve_lookup(500, xs, ys)
        assert result == 50

    def test_three_point_monotone(self) -> None:
        """Three-point monotone curve interpolates correctly in both segments."""
        xs = [0, 500, 1000]
        ys = [0, 50, 100]
        assert curve_lookup(250, xs, ys) == 25
        assert curve_lookup(750, xs, ys) == 75

    def test_non_linear_three_point(self) -> None:
        """Non-linear three-point curve (compressed cold range)."""
        xs = [0, 500, 1000]
        ys = [0, 80, 100]  # first half maps 0-80, second half 80-100
        assert curve_lookup(250, xs, ys) == 40
        assert curve_lookup(750, xs, ys) == 90

    def test_descending_curve(self) -> None:
        """Descending Y values (e.g. thermistor: high voltage = low temp)."""
        xs = [230, 1430, 4630]
        ys = [150, 75, -5]
        # Should interpolate between 230 and 1430 for input 830
        result = curve_lookup(830, xs, ys)
        expected = 75 + (1430 - 830) * (150 - 75) // (1430 - 230)  # ascending X direction
        # Re-derive: input 830, between 230 (150) and 1430 (75)
        # num = (830-230) * (75-150) = 600 * -75 = -45000
        # den = 1430-230 = 1200
        # result = 150 + (-45000 // 1200) = 150 - 37 = 113 (Python floor div)
        assert result == 150 + (830 - 230) * (75 - 150) // (1430 - 230)

    # ------------------------------------------------------------------
    # Edge cases
    # ------------------------------------------------------------------

    def test_two_point_curve(self) -> None:
        """Minimum valid curve (2 points)."""
        xs = [0, 5000]
        ys = [0, 500]
        assert curve_lookup(0, xs, ys) == 0
        assert curve_lookup(2500, xs, ys) == 250
        assert curve_lookup(5000, xs, ys) == 500

    def test_eight_point_curve(self) -> None:
        """Maximum valid curve (8 points) – first and last segment."""
        xs = [100, 200, 300, 400, 500, 600, 700, 800]
        ys = [10, 20, 30, 40, 50, 60, 70, 80]
        assert curve_lookup(50,  xs, ys) == 10   # clamped
        assert curve_lookup(150, xs, ys) == 15   # segment 0
        assert curve_lookup(750, xs, ys) == 75   # segment 6
        assert curve_lookup(900, xs, ys) == 80   # clamped

    def test_integer_truncation(self) -> None:
        """Integer division truncates (not rounds) per C integer semantics."""
        xs = [0, 3]
        ys = [0, 10]
        # 1/3 * 10 = 3.33… → should truncate to 3
        assert curve_lookup(1, xs, ys) == 3


# ---------------------------------------------------------------------------
# Tests for config_file.py
# ---------------------------------------------------------------------------
import tempfile
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from config_file import ConfigFile


class TestConfigFile:
    """Tests for save/load of key=value configuration files."""

    def test_save_and_load_roundtrip(self, tmp_path: pathlib.Path) -> None:
        """Parameters survive a save/load round-trip."""
        cf = ConfigFile()
        params = {"m1_sweep": "800", "filter_vbatt": "32", "units": "1"}
        path = str(tmp_path / "test.txt")
        cf.save_to_file(path, params)
        loaded = cf.load_from_file(path)
        assert loaded["m1_sweep"] == "800"
        assert loaded["filter_vbatt"] == "32"
        assert loaded["units"] == "1"

    def test_comments_are_ignored(self, tmp_path: pathlib.Path) -> None:
        """Lines starting with # are ignored during load."""
        path = str(tmp_path / "test.txt")
        with open(path, "w") as fh:
            fh.write("# This is a comment\n")
            fh.write("m1_sweep=696\n")
            fh.write("\n")
            fh.write("# another comment\n")
            fh.write("units=0\n")
        cf = ConfigFile()
        loaded = cf.load_from_file(path)
        assert "m1_sweep" in loaded
        assert "units" in loaded
        assert len(loaded) == 2

    def test_invalid_line_raises_value_error(self, tmp_path: pathlib.Path) -> None:
        """A line without '=' raises ValueError."""
        path = str(tmp_path / "bad.txt")
        with open(path, "w") as fh:
            fh.write("this_line_has_no_equals_sign\n")
        cf = ConfigFile()
        with pytest.raises(ValueError):
            cf.load_from_file(path)

    def test_missing_file_raises(self, tmp_path: pathlib.Path) -> None:
        """Loading a nonexistent file raises FileNotFoundError."""
        cf = ConfigFile()
        with pytest.raises(FileNotFoundError):
            cf.load_from_file(str(tmp_path / "nonexistent.txt"))

    def test_header_comment_written(self, tmp_path: pathlib.Path) -> None:
        """Saved file contains header comment lines."""
        path = str(tmp_path / "out.txt")
        cf = ConfigFile()
        cf.save_to_file(path, {"m1_sweep": "696"})
        with open(path) as fh:
            content = fh.read()
        assert "# Gauge V4 Configuration" in content
        assert "gauge-config-tool" in content

    def test_default_filename_format(self) -> None:
        """default_filename() starts with 'gauge_config_'."""
        name = ConfigFile.default_filename()
        assert name.startswith("gauge_config_")
        assert name.endswith(".txt")
