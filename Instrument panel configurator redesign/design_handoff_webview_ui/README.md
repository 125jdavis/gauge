# Handoff: Gauge V4 Config Tool, web-view UI (amber theme, dark and light)

## Overview
This replaces the Tkinter UI in `config_tool/gui.py` (repo `125jdavis/gauge`, branch `master`) with an HTML/CSS/JS front end running in a native desktop window through **pywebview**. The serial, config-file, firmware and splash modules stay as they are, and Python remains the authority for validation, unit storage and device I/O.

The new UI has:
- the amber palette `#1B1A17 / #F0E3CA / #FF8303 / #A35709`,
- **Dark** and **Light** themes with a header switch that persists,
- a sidebar with four groups (Device, Inputs, Outputs, Bench) covering the 11 tabs,
- "blueprint" panels with hairline borders and `+` corner marks,
- unsaved-change tracking, a tool-wide unit switch, explanations on disabled fields, and live hardware previews.

The main reason for this route is iteration. Because the front end is plain HTML and CSS, design changes from Claude Design carry over directly, and Claude Design can read `config_tool/web/` back from the repo to design on top of the real UI.

## About the design files
- **`reference/Gauge V4 Config Tool Amber v2.dc.html`** is a **design reference**. Open it in a browser from inside `reference/`; use the Theme switch in its header to compare the two themes. It shows the intended look and behaviour, but it is written in a design-tool format (inline styles, a template runtime, simulated device I/O). Don't ship it. Rebuild it as clean HTML, CSS and JS in `config_tool/web/`.
- **`web/theme.css`** is **production-ready starter CSS**. It holds both themes' tokens and every component and layout class needed, extracted from the reference. Use it as the app stylesheet.
- **`starter/app.py`** and **`starter/api.py`** are a working pywebview skeleton. Device methods marked `TODO` must be filled in by lifting the matching logic from `gui.py`, which is the source of truth for ordering, retries, migration and error handling.

## Fidelity
**High fidelity.** Colours, type, spacing, states and interactions are final. Match the reference exactly; `theme.css` already encodes the values.

## Target architecture
```
config_tool/
  app.py            NEW: pywebview entry point (starter/app.py)
  api.py            NEW: JS bridge (starter/api.py)
  gui_model.py      NEW: every module-level constant and helper moved out of gui.py
  gui.py            keep temporarily: re-export gui_model (`from gui_model import *`) so tests pass; delete once parity is reached
  web/
    index.html      app shell (header, sidebar, page, footer)
    theme.css       from this package
    app.js          state, rendering, events (ES modules, no build step)
    tabs/*.js       one renderer per tab
    units.js        JS ports of raw_to_display / display_to_raw / unit_label
    fonts/          Barlow 400/500 and Barlow Condensed 400/600 woff2 (Google Fonts, SIL OFL; must be bundled because the app works offline)
    icons/          Lucide SVGs, stroke 1.5
  serial_protocol.py, config_file.py, firmware_uploader.py, splash_uploader.py, ssd1306_converter.py, stk500v2.py, intel_hex.py   unchanged
```
- **Front end:** plain HTML plus vanilla JS ES modules, with **no bundler**, so PyInstaller just ships the folder and Claude Design can read and write it as-is. A small no-build library such as Preact + htm via a vendored ESM file is acceptable if rendering gets unwieldy; avoid React/Vite toolchains.
- **Packaging:** update `build_exe.py` to add `--add-data "web;web"` (Windows separator) and add `pywebview` to the requirements. On Windows 10/11 it uses the built-in Edge WebView2 runtime.
- **Tests:** `tests/` imports helpers from `gui`. Moving them into `gui_model.py` and re-exporting from `gui.py` keeps every test passing without edits. Later, point the tests at `gui_model` directly.

## Migration steps
1. Create `gui_model.py`. Move everything above `class ScreenListEditor` in `gui.py` into it, along with any pure helpers inside `GaugeConfigApp` that the bridge needs: the write ordering, the config-file migration, and filter percent↔raw conversion. Re-export it from `gui.py`, then run `pytest`.
2. Add `app.py` and `api.py` from `starter/`. Fill in the `TODO` bodies from `gui.py`'s handlers (connect, write loop, firmware upload, splash upload, Test-tab source override and restore).
3. Build `web/index.html` and the shell (header, sidebar, footer) with `theme.css`. Wire the theme and units switches to `api.get_prefs` and `api.set_pref`.
4. Build the tabs one at a time, in sidebar order, using the specs below and the reference.
5. When every tab matches, switch `build_exe.py` to `app.py` and remove the Tkinter code.

## Bridge API (`window.pywebview.api`)
Wait for the `pywebviewready` event before calling. Every call returns a Promise.

| Method | Returns | Notes |
|---|---|---|
| `get_prefs()` / `set_pref(key, value)` | `{theme, units}` | Stored in `~/.gauge_config_tool.json`. `theme` is `dark` or `light`; `units` is `metric` or `imperial`. |
| `get_static()` | option lists, presets, tooltips, `ALL_PARAMS`, `SOURCE_DEPENDENT_PARAMS`, screens | The UI renders from these, so option lists live only in Python. |
| `list_ports()` | `[[device, description], …]` | |
| `connect(port, baud)` / `disconnect()` | `{connected, version}` | |
| `read_device()` | `{param: raw}` | Raw firmware values, always metric. |
| `write_device(params)` | `{ok, error?}` | Validates first, then writes in `gui.py` order and saves to EEPROM. |
| `load_file()` / `save_file(params)` | params / filename | Native dialogs. Loading migrates old formats. |
| `firmware_files()` / `firmware_update(path)` | list / events | |
| `splash_load(slot)` | `{name, png}` | `png` is a data URL of the 1-bit 128×32 preview. |
| `splash_upload(slot)` / `splash_test(slot)` / `splash_clear(slot)` | events | |
| `tach_preview(payload)` / `tach_preview_stop()` | | |
| `test_signal(name, metricValue)` / `restore_sources()` / `test_odo(revs)` / `ping()` / `version()` | | |

Python pushes events by calling `window.onGaugeEvent({type, …})`:
- `progress`: `{target: 'firmware'|'splash1'|'splash2', pct}`
- `log`: `{target, text}`
- `status`: `{text}`
- `disconnected`: no payload

## Parameter mapping
The reference uses shorthand keys. Implement against the real names in `ALL_PARAMS`:

| Reference key | Firmware param | Conversion (UI → stored) |
|---|---|---|
| `m{n}_angle` | `m{n}_sweep` | degrees × 12 steps (`_angle_to_steps`), range 8.3–315° |
| `m{n}_lo` / `m{n}_hi` / `m{n}_dir` | `m{n}_min` / `m{n}_max` | CCW swaps min and max (`_gauge_range_to_device` / `_from_device`); display units by function kind |
| `filter_*` (0–99 %) | `filter_*` (0–127) | `_filter_percent_to_raw` / `_filter_raw_to_percent` |
| `hall_speed_min` | same | km/h × 100 |
| `speedo_max` | same | mph × 100 (`_speedo_max_to_raw`) |
| `av{n}_vlo` / `av{n}_vhi` | `av{n}_adc_lo` / `av{n}_adc_hi` | volts → ADC (`volts_to_adc`) |
| `av{n}_plo` / `av{n}_phi` | `av{n}_phys_lo` / `av{n}_phys_hi` | `physical_to_raw` (kPa × 10 absolute, or Lambda × 1000) |
| `therm_t{i}`, `therm_r{i}` | same | temperatures stored as whole °C; `therm_v{i}` computed via `thermistor_curve` |
| `fuel_r_*`, `fuel_mid_pct` | same | `fuel_v_*` computed with `divider_millivolts` |
| `oil_warn` / `coolant_warn` | `oil_warn_kpa` / `coolant_warn_c` | |
| `oil_delay` … `fuel_delay` (s) | `oil_warn_ms` … `fuel_warn_ms` | `debounce_seconds_to_ms` |
| `d1` / `d2` (order and enabled) | `disp1_order` / `disp2_order` | enabled IDs in order, e.g. `"5,1,11"` (`parse_screen_order`) |
| `warn_leds`, `shift_leds`, `tach_*`, `out{n}_*` | same | |
| `port`, `baud`, `fwPath`, `test_*`, `odo_revs` | (not params) | UI-only state |

## State management (JS)
- `raw`: `{param: string}`, the last values read from the device or a file. Always metric firmware units.
- `edits`: `{param: string}`, the user's pending changes in raw units. A parameter is **dirty** when `param in edits && edits[param] !== raw[param]`.
- `prefs`: `{theme, units}`.
- `conn`: `{connected, port, baud, version}`.
- `tab`: one of `connection | source | speed | sensors | motors | tach | outputs | display | faults | splash | test`.
- Transient: preview RPM, tach-test on/off, firmware/splash progress and logs, status text.
- **Display conversion happens only at render and parse time.** Read the value with `raw_to_display(edits[p] ?? raw[p], kind, imperial)`; on input, store `display_to_raw(text)`. Switching units never changes stored values; it re-renders. (The reference converts values in place, but storing raw values is the correct model.)
- **Tab dirty counts** come from a map of parameter → tab. Write sends the merged `{...raw, ...edits}`, and success clears `edits`. Read and Load replace `raw` and clear `edits`.

## Screens / views
Class names refer to `web/theme.css`. Exact copy for every tab title, description and option label is in the reference's logic class (`NAV`, `DESC`) and in `get_static()`.

### App shell (`.app`)
- **Header (`.app-header`)**, left to right:
  - `.app-title`: **GAUGE V4** + "Configuration Tool".
  - `.conn`: gets `.is-on` when connected. Text is `Connected · COM3 · 115200` or `Disconnected`.
  - **Theme** `.seg` (Dark | Light). Sets `<html data-theme>` and calls `set_pref('theme')`.
  - **Tool units** `.seg` (Metric | 'Merican).
  - `.actions`: `Load from file…` (`.btn-ghost`, folder-open), `Save to file…` (`.btn-ghost`, save), `Read from device` (`.btn-secondary`, download), `Write to device` (`.btn-primary.blueprint` with four corners, upload icon, plus `.count` while edits exist). Device buttons are disabled when offline.
- **Body (`.app-body`)**: a 200 px `.sidebar` and the `.page-scroll` area.
  - Sidebar: `.nav-group` → `h6` + `.nav-item` buttons. The active item gets `aria-current="page"`; tabs with unsaved changes show a `.badge`.
- **Footer (`.app-footer`)**: status text · `N unsaved changes` or `No unsaved changes` · `gauge-config-tool v0.1`.

### Page (`.page`)
- `.page-head`: `h6` kicker (`01 / DEVICE`), `h2` title, `p` description.
- Then a `.panel-grid` of `.panel.blueprint` cards. Each card has four `<i class="corner tl|tr|bl|br">` and gets `.wide` when it spans the full width.
- Each panel:
  - `.panel-head` with an `h4` title and a `.panel-tag`.
  - Optional `.panel-row` holding a dial plus `.fields`.
  - Each `.field` contains a `label` (with a `.dirty-dot` when dirty), a `.control` (an input, a select, or a range + `NN%` readout), a `.unit`, and an optional `.note`.
  - Inputs get `.is-dirty` when changed.
  - When a field depends on a source that isn't selected, it gets `aria-disabled="true"`, its control is `disabled`, and the `.note` reads, for example, *"Used when Speed source is Hall sensor. Currently CAN."* (computed from `SOURCE_DEPENDENT_PARAMS`).

### Tabs
1. **Connection** (Device)
   - **Serial Connection** panel: Port select, Baud select, Status, Firmware; buttons Refresh ports and Connect/Disconnect (primary).
   - **Firmware Update** panel (tag `.hex · no toolchain required`): file field, Browse…, Update Firmware (primary), progress bar, log.
2. **Signal Source** (Inputs): panels for CAN Bus (protocol), Speed Source, RPM Source, and Other Signal Sources (seven selects).
3. **Speed/RPM**: panels for Speed & Hall Sensor, Engine RPM Sensor, and Mechanical odometer calibration. Hall and coil fields depend on their source.
4. **Sensors**
   - **Thermistor** (wide): Curve preset, Bias, and Filter fields, plus a P1–P6 table with rows for Temperature, Resistance, and Voltage (mV, computed).
   - **Fuel Level Sender**: Empty/Mid/Full table.
   - **Analog Sensor Filters**.
   - **Analog Sensor Types and Calibration** (wide): an AV1–AV3 table with Type and V low/high/physical low/high columns.
   - Editing any preset table cell switches its preset to Custom.
5. **Motor Config** (Outputs)
   - Motor 1–4 panels, each with a dial plus Function, Sweep angle, Low, High, and Direction.
   - Motor S (tag `NEMA14`), Motor S - Zeroing, and Startup sweep panels.
   - Changing a motor's Function resets Low and High to `MOTOR_FUNCTION_RANGES`.
6. **LED Tach**: a wide LED Tachometer panel with fields, then the **Strip preview** panel (see below).
7. **Outputs**: Output 1 (D12) and Output 2 (D13), both wide.
   - Mode: Disabled / On/Off / PWM.
   - **On/Off:** a table with "Turn ON when value" [> or <] [value] [unit] and "Turn OFF when value" [opposite operator] [value].
   - **PWM:** Frequency plus an 8-point Value/Duty table and a curve chart.
   - Validation errors from `validate_outputs` appear in `.error-box`.
8. **Display Settings**
   - Units and Clock & Fuel panels.
   - Two screen-list panels (`Display N screens`, tag `N enabled`). Each `.screen-row` has a checkbox, position number, name, and up/down `.btn-ghost.btn-icon.sm` buttons. Display 1 ends with a fixed "Settings · always last" row.
   - At least one screen must stay enabled.
9. **Fault Warnings**: a wide table with Flash when / Threshold / unit / Delay (s) columns for Oil pressure (below), Engine temp (above), Battery voltage (below), and Low fuel (below), plus an Engine running RPM field.
10. **Splash Screens**: two slots. Each has an `.oled` preview (or `EMPTY SLOT`), a caption, progress, and the buttons Load image…, Upload to device (primary), Test on device, and Clear slot (ghost).
11. **Test** (Bench)
    - **Signal Injection** panel, with Restore sources now in the header. One row per signal: label + `source: X` / range / number input / unit / Send.
    - **Other Tests** panel: Odometer motor revs field; buttons Ping, Version, Move odometer, Splash slot 1, Splash slot 2.
    - **Log** panel.

### Hardware previews (inline SVG or DOM, styled by `theme.css`)
- **Motor dial (`svg.dial`)**: viewBox `0 0 120 108`, centred at (60, 60), radius 44, 0° at 12 o'clock.
  - `.track`: the full 315° arc.
  - `.sweep`: the sweep-angle arc, centred on 12 o'clock.
  - `.needle`: from the centre to the sweep start, or the sweep end when CCW.
  - `.hub`: radius 3.5.
- **LED strip (`.strip`)**
  - Layout: one `.row` of `.led` circles and a second row of `.zone` bars.
  - Lit LEDs use the zone's display colour from `tach_display_color` (screen brightness 60–100%); unlit LEDs use `--led-off`.
  - Step mapping comes from `tach_steps`, `tach_led_step` and `tach_step_rpm` (port them to JS).
  - Zones: a step is **shift** if `step ≥ steps−1−shift_leds`, otherwise **warning** if `step ≥ steps−1−warn_leds`, otherwise **normal**.
  - Shift LEDs blink at 300 ms while the preview RPM is at or above `tach_flash`.
  - Below the strip:
    - a legend with each zone's RPM range,
    - a Preview RPM range (0 to max(tach_max, tach_flash) + 1000) with a 24 px readout,
    - Warning and Shift zone steppers,
    - a Fit zones to strip button,
    - three colour editors (Hue 0–359, Brightness 0–100 via `tach_hsv_color`),
    - a `Test strip: On/Off` toggle that calls `tach_preview`, with the note "Test uses unsaved settings; leaving this tab restores normal output."
- **PWM chart (`svg.chart`)**: viewBox `0 0 480 156`. Plot x from 44 to 468 and y = 130 − duty × 1.2. Gridlines at 0/50/100%, min and max labels at the bottom.
- **OLED (`.oled`)**: shows the `png` from `splash_load`, scaled with pixelated rendering.

## Interactions & behaviour
- **Theme:** default dark, persisted. Applied by setting `document.documentElement.dataset.theme` before first paint: read the prefs, then reveal the page. Set the pywebview `background_color` to the dark bg so the window doesn't flash.
- **Units:** the status reads "Tool units set to … The firmware always stores metric." Unit labels come from `unit_label`, and the rounding matches `raw_to_display`.
- **Dependent fields** and **presets** behave as described under Screens. Validation errors block Write and appear in the panel's error box.
- **Leaving tabs:** leaving LED Tach stops the strip test (`tach_preview_stop`). Leaving Test calls `restore_sources()`. So do disconnecting, Read, Write and closing the window (`on_closing`).
- **Hover, focus and pressed states** are all in `theme.css`. Keyboard focus is a 2 px accent ring. Disabled elements drop to 45% opacity (buttons) or 50% (field rows).
- **Window:** minimum 800×600. The panel grid reflows to one column below about 830 px of content width.

## Design tokens
All tokens are defined in `web/theme.css` (`:root` / `[data-theme]`).

| Token | Dark | Light |
|---|---|---|
| `--color-bg` | #1B1A17 | #F0E3CA |
| `--color-surface` | #24221e | #e6d7b9 |
| `--color-text` | #F0E3CA | #1B1A17 |
| `--color-accent` | #FF8303 | #FF8303 |
| `--color-accent-700` | #ffb366 | #A35709 |
| `--color-accent-100` | #2e2013 | #fbe3c2 |
| `--color-neutral-700` | #b6ad9c | #5d5546 |
| `--color-neutral-800` | #d4c9b4 | #423c31 |
| `--color-divider` | text at 18% | text at 18% |
| `--color-hairline` | text at 8% | text at 8% |

- **Type:**
  - Barlow Condensed 600 for headings, nav, buttons and kickers; Barlow 400/500 for body text.
  - Sizes: 29 (page title), 20 (app title), 17.5 (panel title), 16 (nav), 13.5 (inputs, buttons, description), 13 (labels), 12 (units, notes), 11 (tags, table heads).
- **Spacing:** 3.4 / 6.8 / 10.2 / 13.6 / 20.4 / 27.2 px. Panels: padding 17 × 19 px, 12 px between children. Panel grid gaps: 23 px between rows, 18 px between columns. Inputs are 33 px tall (29 px compact).
- **Radius:** 0 everywhere. **No shadows.**
- **Icons:** Lucide, stroke 1.5, 16 px: folder-open, save, download, upload, triangle-alert, chevron-up, chevron-down.

## Keeping Claude Design in the loop
- Keep `config_tool/web/` readable: semantic HTML, the classes from `theme.css`, no minification, and no build output committed.
- New visual work goes in `theme.css` as tokens or classes, not inline styles.
- After each round, Claude Design can sync from the repo, read `web/`, and propose changes as HTML/CSS that map straight back.

## Files in this package
- `reference/Gauge V4 Config Tool Amber v2.dc.html` (plus `support.js` and `_ds/`): the interactive design reference with the theme switch.
- `web/theme.css`: the production stylesheet with both themes.
- `starter/app.py`: the pywebview entry point.
- `starter/api.py`: the bridge skeleton mapped onto the existing modules.
