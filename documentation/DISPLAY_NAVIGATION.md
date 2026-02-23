# Display Navigation Guide

## Display 1 — Screen Order (Scrolling)

Rotating the encoder **clockwise** advances through screens in the order below.  
The internal index runs **0 → 17**, wrapping back to 0 after position 17.  
Settings (0) is placed at the end of the table because `dispArray1[0]` initialises  
to `1` (Oil Pressure), so in normal use the sequence experienced is:  
Oil Pressure → … → Boost (text) → Settings → Oil Pressure → …

| Position | Screen |
|----------|--------|
| 1 | Oil Pressure |
| 2 | Coolant Temperature |
| 3 | Oil Temperature |
| 4 | Fuel Level |
| 5 | Battery Voltage |
| 6 | Clock |
| 7 | Trip Odometer (press button to reset) |
| 8 | Speed |
| 9 | RPM |
| 10 | Ignition Timing |
| 11 | Air/Fuel Ratio |
| 12 | Fuel Pressure |
| 13 | Fuel Composition (ethanol %) |
| 14 | Injector Duty Cycle |
| 15 | Falcon Script Logo |
| 16 | Boost Gauge (icon + bar graph) |
| 17 | Boost (icon + text readout) |
| 0 | **Settings** (press button to enter) |

> **Note:** `dispArray1[0]` initialises to `1` (Oil Pressure) on first power-up.  
> `goToLevel0()` returns to position `0` (Settings) after exiting any submenu.

---

## Display 2 — Selection Options (Settings → Display 2)

Inside the Settings menu, scrolling through the **Display 2** submenu selects  
what is permanently shown on the second OLED. The options appear in this order:

| Index | Screen shown on Display 2 |
|-------|--------------------------|
| 0 | Oil Pressure |
| 1 | Coolant Temperature |
| 2 | Battery Voltage |
| 3 | Fuel Level |
| 4 | RPM |
| 5 | Speed |
| 6 | Clock |
| 7 | 302 CID Logo |
| 8 | 302V Logo |
| 9 | Falcon Script Logo |
| 10 | Boost Gauge (icon + bar graph) |
| 11 | Boost (icon + text readout) |

> Press the button on any option to save it and return to the main menu.  
> The selection is stored in `dispArray2[0]` and persisted to EEPROM.
