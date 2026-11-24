"""
datalog.py
==========
Live data logging: the gauge's "L," stream lines, unit conversion and CSV recording.

The firmware ("log start", see serial_config.cpp serialLogUpdate) sends one line every
100 ms while logging is on::

    L,<millis>,<speed km/h*100>,<rpm>,<oil kPa*10>,<fuel kPa*10>,<lambda*1000>,
      <coolant C*10>,<MAP kPa abs*10>,<battery V*100>

Values travel in metric firmware units. The page converts them for display, and the
CSV is written in the units the tool was set to when recording started.
"""

from __future__ import annotations

import csv
from typing import IO, List, Optional, Tuple

LOG_PREFIX = "L,"

# (key, name, metric unit, imperial unit, device scale, kind, minimum trace span (metric),
#  decimals (metric, imperial)). Order matches the firmware's data line.
LOG_SIGNALS = (
    ("speed", "Vehicle speed", "km/h", "mph", 100, "speed", 10, (0, 0)),
    ("rpm", "Engine RPM", "rpm", "rpm", 1, None, 500, (0, 0)),
    ("oil", "Oil pressure", "kPa", "psi", 10, "pressure", 50, (0, 1)),
    ("fuel", "Fuel pressure", "kPa", "psi", 10, "pressure", 50, (0, 1)),
    ("lambda", "Lambda", "λ", "λ", 1000, None, 0.1, (3, 3)),
    ("coolant", "Coolant temp", "°C", "°F", 10, "temp", 5, (1, 1)),
    ("map", "MAP", "kPa (abs)", "psi (abs)", 10, "pressure", 20, (0, 1)),
    ("vbatt", "Battery voltage", "V", "V", 100, None, 1, (2, 2)),
)

KMH_PER_MPH = 1.609344
PSI_PER_KPA = 0.1450377377


def to_units(kind: Optional[str], metric: float, imperial: bool) -> float:
    """Convert one metric firmware value for display."""
    if not imperial or kind is None:
        return metric
    if kind == "speed":
        return metric / KMH_PER_MPH
    if kind == "pressure":
        return metric * PSI_PER_KPA
    if kind == "temp":
        return metric * 1.8 + 32
    raise ValueError(f"unknown kind {kind!r}")


def parse_line(line: str) -> Optional[Tuple[int, List[float]]]:
    """(gauge millis, metric values in LOG_SIGNALS order), or None if *line* is not a full data line."""
    if not line.startswith(LOG_PREFIX):
        return None
    parts = line[len(LOG_PREFIX):].split(",")
    if len(parts) != 1 + len(LOG_SIGNALS):
        return None  # truncated or merged line
    try:
        ms = int(parts[0])
        raw = [int(p) for p in parts[1:]]
    except ValueError:
        return None
    return ms, [r / spec[4] for r, spec in zip(raw, LOG_SIGNALS)]


class CsvRecorder:
    """Writes samples as CSV rows: time (s since the first row), then every signal."""

    FLUSH_EVERY = 10  # rows (one second at 10 Hz)

    def __init__(self, stream: IO[str], imperial: bool) -> None:
        self._stream = stream
        self._imperial = imperial
        self._writer = csv.writer(stream, lineterminator="\n")
        self._segment_ms: Optional[int] = None  # gauge millis at the start of this run
        self._base_s = 0.0                      # elapsed time when that run started
        self._last_s = 0.0
        self.rows = 0
        units = 3 if imperial else 2
        self._writer.writerow(["Time (s)"] + [f"{s[1]} ({s[units]})" for s in LOG_SIGNALS])

    def write(self, ms: int, values: List[float]) -> None:
        if self._segment_ms is None:
            self._segment_ms = ms
        elif ms < self._segment_ms + (self._last_s - self._base_s) * 1000:
            # The gauge restarted (millis went back): keep time increasing.
            self._base_s = self._last_s + 0.1
            self._segment_ms = ms
        self._last_s = self._base_s + (ms - self._segment_ms) / 1000
        row = [f"{self._last_s:.3f}"]
        for value, spec in zip(values, LOG_SIGNALS):
            decimals = spec[7][1 if self._imperial else 0]
            row.append(f"{to_units(spec[5], value, self._imperial):.{decimals}f}")
        self._writer.writerow(row)
        self.rows += 1
        if self.rows % self.FLUSH_EVERY == 0:
            self._stream.flush()

    def close(self) -> None:
        self._stream.close()
