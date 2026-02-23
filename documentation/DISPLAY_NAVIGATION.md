# Display Navigation Guide

## Display 1 — Screen Order (Scrolling)

Rotating the encoder **clockwise** advances through screens in the order below.  
The internal index runs **0 → 17**, wrapping back to 0 after position 17.  
Settings (0) is placed at the end of the table because `dispArray1[0]` initialises  
to `1` (Oil Pressure), so in normal use the sequence experienced is:  
Oil Pressure → … → Falcon Script Logo → Settings → Oil Pressure → …

| Position | Screen |
|----------|--------|
| 1 | Oil Pressure |
| 2 | Coolant Temperature |
| 3 | Fuel Level |
| 4 | Battery Voltage |
| 5 | RPM |
| 6 | Speed |
| 7 | Air/Fuel Ratio |
| 8 | Fuel Pressure |
| 9 | Boost Gauge (icon + bar graph) |
| 10 | Boost (icon + text readout) |
| 11 | Oil Temperature |
| 12 | Fuel Composition (ethanol %) |
| 13 | Injector Duty Cycle |
| 14 | Ignition Timing |
| 15 | Trip Odometer (press button to reset) |
| 16 | Clock |
| 17 | Falcon Script Logo |
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
| 2 | Fuel Level |
| 3 | Battery Voltage |
| 4 | RPM |
| 5 | Speed |
| 6 | Boost Gauge (icon + bar graph) |
| 7 | Boost (icon + text readout) |
| 8 | Clock |
| 9 | Falcon Script Logo |

> Press the button on any option to save it and return to the main menu.  
> The selection is stored in `dispArray2[0]` and persisted to EEPROM.
