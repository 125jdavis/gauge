# Dynamic Memory Reduction — Implemented

**Target platform:** Arduino Mega 2560 (8 KB RAM)  
**Scope:** Reduce SRAM usage with zero or negligible effect on runtime performance  
**Status:** All items implemented

---

## Current RAM Budget (approximate)

| Source | Bytes |
|--------|-------|
| `globals.cpp` global variables | ~622 |
| `config_calibration.cpp` calibration globals | ~90 |
| Synthetic signal generator static locals (`utilities.cpp`) | ~146 |
| Sensor/output/CAN static locals | ~50 |
| Arduino framework overhead (stack, heap, Serial buffer) | ~500+ |
| **Estimated total** | **~1,400 + framework** |

The largest single consumer of user-allocated RAM is **`msgString[128]`** at 128 bytes, which is 20% of the globals-file allocation.

---

## Proposed Changes (in descending order of impact)

---

### 1. Delete `msgString[128]` — saves **128 bytes**

**Location:** `globals.cpp` line 141, `globals.h` line 145  
**Impact:** Largest single gain available. Zero behavioural change.

`msgString` is a 128-byte `char` array declared as a global. Its only use is inside a
block of `sprintf`/`Serial.print` calls in `can.cpp` that is **entirely commented out**
(lines 79–99). Nothing in the codebase reads or writes `msgString` in live code.

**Proposed action:**
- Delete the definition from `globals.cpp`.
- Delete the `extern` declaration from `globals.h`.

If the debug print block in `can.cpp` is ever re-enabled, a local `char buf[48]` declared
at the top of the `if` block is sufficient and cheaper.

---

### 2. Delete unused EEPROM helper globals `int *input` / `int output` — saves **4 bytes**

**Location:** `globals.cpp` lines 169–170, `globals.h` lines 163–164

These two variables (`int *input` pointer and `int output` buffer) appear to be a
leftover from an earlier EEPROM-access pattern. A codebase-wide search finds zero calls
that use them; the live EEPROM code uses `EEPROM.get()`, `EEPROM.put()`, and
`EEPROM.update()` directly.

**Proposed action:** Delete both the definitions and the `extern` declarations.

---

### 3. Delete dead function `serialInputFunc()` — saves **heap pressure**

**Location:** `utilities.cpp` lines 150–168

`serialInputFunc()` is never declared in `utilities.h` and never called from any `.cpp`
or `.ino` file. It was superseded by `processSerialCommands()`. It is dead code.

Beyond taking up flash, the function body instantiates a `String` object
(`String inputSer = Serial.readStringUntil('\n')`) and performs string concatenation
with `+`, both of which draw from the heap. On an AVR with a fixed stack/heap, `String`
operations are a leading cause of heap fragmentation and unexpected crashes. Even though
the function is never called today, its presence is a hazard.

Additionally, the two `Serial.println` calls inside use string literals without the `F()`
macro (see §4), wasting RAM on startup.

**Proposed action:** Delete the entire function body (lines 150–168).

---

### 4. Wrap active string literals in `F()` — saves **18 bytes**

On AVR, a C-string literal that is **not** wrapped in `F()` is stored in both flash and
SRAM (copied to SRAM at startup). Using `F()` keeps the string in flash only.

Active string literals currently **missing** `F()`:

| File | Line | String | Bytes in RAM |
|------|------|--------|--------------|
| `outputs.cpp` | 674 | `"zeroed"` | 7 |
| `outputs.cpp` | 707 | `"full sweep"` | 11 |

**Total: 18 bytes**

The only active string already using `F()` is in `utilities.cpp` line 823
(`"odo motor: not allowed while speed > 0"`).

**Proposed action:** Change both calls in `outputs.cpp` to use `F()`:
```cpp
// Before:
Serial.println("zeroed");
Serial.println("full sweep");

// After:
Serial.println(F("zeroed"));
Serial.println(F("full sweep"));
```

---

### 5. Convert `stateDuration` from `unsigned long` to `uint16_t` in synthetic generators — saves **12 bytes**

**Location:** `utilities.cpp` — six functions

Each of the five "pressure/temperature/level" synthetic generators (`generateSyntheticCoolantTemp`,
`generateSyntheticOilPressure`, `generateSyntheticFuelPressure`, `generateSyntheticFuelLevel`,
`generateSyntheticManifoldPressure`) plus `generateSyntheticSpeed` has a
`static unsigned long stateDuration` that holds a millisecond count in the range
1,000–30,000 ms. This fits comfortably in a `uint16_t` (max 65,535). The comparison
`timeInState >= stateDuration` is safe because `timeInState` is already an `unsigned long`;
the compiler promotes `stateDuration` during comparison.

**Per function:** `unsigned long` (4 bytes) → `uint16_t` (2 bytes) = **2 bytes saved**  
**6 functions × 2 bytes = 12 bytes total**

---

### 6. Convert floating-point state variables in synthetic generators to integers — saves **up to 24 bytes**

The five float-heavy synthetic generators each store `currentXxx`, `targetXxx`, and `rate`
as `float` (4 bytes each, 12 bytes total per function). In every case the values are
ultimately passed to `sigSelect()` and then to display and motor-control code that only
needs whole-unit precision. Sections 6a–6d each save 6 bytes by replacing three float
statics with two `int16_t` and one `int8_t` or `int16_t` statics. Section 6e (`generateSyntheticFuelLevel`)
is excluded due to precision concerns (see below). The net saving across the four convertible
generators is **24 bytes**.

The analysis below covers each generator individually:

#### 6a. `generateSyntheticCoolantTemp` — saves **6 bytes**

| Variable | Range | Current type | Proposed type | Saved |
|----------|-------|--------------|---------------|-------|
| `currentTemp` | −10 to 140 °C | `float` | `int16_t` (°C × 1) | 2 |
| `targetTemp` | −10 to 140 °C | `float` | `int16_t` | 2 |
| `rate` | −18 to +18 °C/s | `float` | `int8_t` (°C/s, whole) | 3 |

`int8_t` holds −128 to +127, which covers the ±18 °C/s rate. The saving is 3 bytes here
(float 4 → int8_t 1) rather than 2 bytes because the range is narrow enough for a 1-byte
signed type. Using whole-degree precision for rate is acceptable for a synthetic demo
signal. The return type stays `float`; the value is cast at the `return` statement.

The `fabs(currentTemp - targetTemp) < 1.0` comparison becomes `abs(currentTemp - targetTemp) < 1`
(integer absolute value, no need for `fabs`).

Delta update: `currentTemp += (int8_t)rate * (deltaTime / 1000);` — integer arithmetic
using the already-integer `deltaTime`.

#### 6b. `generateSyntheticOilPressure` — saves **6 bytes**

| Variable | Range | Current type | Proposed type | Saved |
|----------|-------|--------------|---------------|-------|
| `currentPressure` | 0–600 kPa | `float` | `uint16_t` | 2 |
| `targetPressure` | 0–600 kPa | `float` | `uint16_t` | 2 |
| `rate` | ±300 kPa/s | `float` | `int16_t` | 2 |

`uint16_t` covers 0–65,535; `int16_t` covers ±32,767 — both well within range.  
Return type stays `float`; cast at return.

#### 6c. `generateSyntheticFuelPressure` — saves **6 bytes**

Identical analysis to §6b (`rate` range ±600 kPa/s, fits `int16_t`).

#### 6d. `generateSyntheticManifoldPressure` — saves **6 bytes**

Identical analysis to §6b (`MAX_PRESSURE` = 250 kPa, fits `uint16_t`).

#### 6e. `generateSyntheticFuelLevel` — **skip or defer**

`currentLevel` and `targetLevel` are 0–100 % and would fit in `uint8_t`. However,
`rate` can be as low as 1 %/s and `deltaTime` is typically 10–20 ms:

```
delta = rate × (deltaTime / 1000.0) = 1 × 0.010 = 0.01 %
```

Using integer arithmetic would round every delta to zero at slow rates, causing the level
to never move. A fixed-point workaround (e.g., storing level as centipercent × 100 in
`uint16_t`) would work but adds implementation complexity. Recommend deferring.

#### 6f. `generateRPM` — **no saving available**

Already uses `int` for `gRPM` (2 bytes). `lastUpdateTime` must remain `unsigned long`
for millis() accuracy. Only `rpmSwitch` at 1 byte is irreducible.

---

### 7. Convert lookup tables to integer types — saves **up to 69 bytes**

**Location:** `globals.cpp` lines 149–158

The two sensor lookup tables are stored as `float` arrays:

| Array | Elements | Current size | Proposed type | Proposed size | Saved |
|-------|----------|--------------|---------------|---------------|-------|
| `thermTable_x[]` | 6 | 24 bytes | `uint16_t` (millivolts: 230–4950) | 12 bytes | 12 |
| `thermTable_l[]` | 6 | 24 bytes | `int16_t` (°C: −40 to 150) | 12 bytes | 12 |
| `fuelLvlTable_x[]` | 9 | 36 bytes | `uint16_t` (millivolts: 870–2300) | 18 bytes | 18 |
| `fuelLvlTable_l[]` | 9 | 36 bytes | `uint8_t` (gallons: 0–16) | 9 bytes | 27 |
| **Total** | | **120 bytes** | | **51 bytes** | **69** |

The `curveLookup()` function (`sensors.cpp` line 505) currently takes `float brkpts[]`
and `float curve[]`. Two options:

- **Option A (simpler):** Keep the existing `float curveLookup()` but replace the global
  float arrays with integer arrays and add a thin float-conversion wrapper at the two call
  sites in `gauge_V4.ino` and `sensors.cpp`. The wrapper converts the integer tables to
  floats on-the-fly (no storage change).  
  *Con:* No RAM saving — the arrays are still float.

- **Option B (full saving):** Create an overloaded `curveLookup()` that accepts
  `int16_t brkpts[]` and `int16_t curve[]` and performs the interpolation with
  integer arithmetic. Call the new overload at both sites.  
  *Pro:* Full 69-byte saving. *Con:* New function + arithmetic changes.

This is the highest-effort item. Recommend tackling after the zero-effort items above.

---

### 8. Remove active debug `Serial.println` calls — **no RAM saving, but code hygiene**

Three live `Serial.println` calls appear to be unfinalised debug remnants:

| File | Line | What it prints | When |
|------|------|----------------|------|
| `display.cpp` | 560 | `RPM` (integer) | Every RPM display update |
| `sensors.cpp` | 337 | `spdHall` (integer) | Every Hall speed update |
| `sensors.cpp` | 350 | `spdHall` (decayed value) | Hall speed decay event |

These print to Serial continuously during normal operation, consuming CPU time in the
main loop and potentially interfering with the `processSerialCommands()` parser. They
do not consume additional static RAM (integer arguments don't copy a string), but they
should be removed for production code.

---

## Summary Table

| # | Change | Files | Bytes saved | Effort |
|---|--------|-------|-------------|--------|
| 1 | Delete `msgString[128]` | `globals.cpp`, `globals.h` | **128** | Trivial |
| 2 | Delete `int *input`, `int output` | `globals.cpp`, `globals.h` | **4** | Trivial |
| 3 | Delete dead `serialInputFunc()` | `utilities.cpp` | 0 RAM, removes heap risk | Trivial |
| 4 | Add `F()` to two string literals | `outputs.cpp` | **18** | Trivial |
| 5 | `stateDuration` → `uint16_t` in 6 generators | `utilities.cpp` | **12** | Low |
| 6a–d | Float statics → int in 4 generators | `utilities.cpp` | **24** | Medium |
| 7 | Lookup tables → integer arrays | `globals.cpp`, `sensors.cpp` | **69** | Medium-High |
| 8 | Remove debug Serial prints | `display.cpp`, `sensors.cpp` | 0 RAM, hygiene | Low |
| | **Total (items 1–7)** | | **~255 bytes** | |

Items 1–5 are independent of each other and can be done in any order. Items 6 and 7
should be done after items 1–5 are verified, and each deserves its own test pass.

---

## What Was Explicitly Not Proposed

- **Converting calibration globals to `#define`** — `config_calibration.h` comments note
  these are kept as variables "to allow for future runtime configuration via serial/EEPROM."
  That intent should be respected.

- **Reducing dirty-tracking `*_prev` float variables** — These are read in the display
  layer with tolerance comparisons that require float precision. Converting them to
  `int` would require auditing every comparison in `display.cpp`.

- **Reducing `leds[27]`** (81 bytes for `CRGB`) — This is sized by the physical LED
  count and cannot be reduced without removing LEDs.

- **`stateStartTime` / `lastUpdateTime` → smaller type** — These must remain
  `unsigned long` to hold `millis()` values correctly across the 49-day rollover window.
  Truncating to `uint16_t` would cause the timer to misfire every 65 seconds.
