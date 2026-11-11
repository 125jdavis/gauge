# Display Navigation Guide

## Screen IDs

Both displays use the same screen IDs (Settings is display 1 only):

| ID | Screen |
|----|--------|
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
| 13 | *(retired – Injector Duty, no longer selectable)* |
| 14 | *(retired – Ignition Timing, no longer selectable)* |
| 15 | Trip Odometer (display 1: press button to reset) |
| 16 | Clock |
| 17 | Falcon Script Logo |
| 0 | **Settings** (display 1 only; press button to enter) |

## Choosing and ordering screens (config tool)

In the config tool's **Display Settings** tab, the *Display 1 screens* and
*Display 2 screens* boxes list every screen with a checkbox. Only checked
screens are used; drag a row up or down to change the order. At least one
screen must stay checked in each box.

The lists are stored in the firmware as `disp1_order` and `disp2_order`
(comma-separated screen IDs, e.g. `1,5,3`) and persisted to EEPROM
(addresses 20-37 and 38-55). If the stored lists are missing or invalid,
all screens are enabled in numeric order.

## Display 1 — Scrolling

Rotating the encoder steps through the checked display 1 screens in the
configured order, followed by **Settings**, then wraps around. Settings
cannot be removed or reordered.

## Display 2 — Selection (Settings → Display 2)

Display 2 does not scroll in normal use. Enter **Settings → Display 2** and
rotate the encoder through the checked display 2 screens, in the configured
order. Press the button to select one; the selection is stored in
`dispArray2[0]` (a screen ID) and persisted to EEPROM. If the saved screen is
no longer in the list, the first listed screen is used.
