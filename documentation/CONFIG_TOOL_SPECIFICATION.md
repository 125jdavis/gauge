# Arduino Gauge Configuration Tool — Complete Technical Specification

**Versions covered: v1.0 · v1.1 · v1.2**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Memory Requirements](#2-memory-requirements)
3. [Serial Communication Protocol](#3-serial-communication-protocol)
4. [Data Structures](#4-data-structures)
5. [Configuration Parameters](#5-configuration-parameters)
6. [Calibration Curves](#6-calibration-curves)
7. [Motor Assignment System](#7-motor-assignment-system)
8. [Filter System](#8-filter-system)
9. [Display Screen Configuration](#9-display-screen-configuration)
10. [Custom Splash Screen Feature](#10-custom-splash-screen-feature)
11. [Arduino Implementation Guide](#11-arduino-implementation-guide)
12. [Python Application Specification](#12-python-application-specification)
13. [Firmware Upload Procedure](#13-firmware-upload-procedure)
14. [Implementation Checklists](#14-implementation-checklists)
15. [Testing Plan](#15-testing-plan)

---

## 1. Overview

### Project Description

This specification describes a **PC-based configuration tool** for the Arduino Mega 2560–based instrument panel controller used in vintage vehicle retrofits. The tool connects via USB serial and allows non-programmer end-users to read and write all calibration parameters, lookup curves, display settings, and custom images without modifying or re-flashing firmware.

The Arduino firmware (gauge_V4) already contains all runtime logic. The PC tool is a **thin configuration layer**: it reads current values, presents a friendly UI, validates inputs, and writes back over a defined serial protocol.

### Goals and Objectives

| Goal | Description |
|------|-------------|
| Zero-edit configuration | Change all tunable parameters without touching C++ source |
| Non-destructive workflow | Read-modify-write; never silently corrupt EEPROM |
| Cross-platform GUI | Works on Windows, macOS, Linux (Python + tkinter or PyQt5) |
| Extensible protocol | Human-readable command strings; easy to add new parameters |
| Custom branding | Upload user-supplied 128×32 monochrome splash images |

### System Architecture Overview

```
┌────────────────────────────────────────────────────────────┐
│                      PC Application                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │ Motor    │  │ Sensors  │  │ Display  │  │  Splash   │  │
│  │ Config   │  │ & Curves │  │ Settings │  │  Upload   │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └─────┬─────┘  │
│       └─────────────┴─────────────┴───────────────┘        │
│                     Serial Protocol Layer                    │
│           (115200 baud, human-readable commands)            │
└────────────────────────────────┬───────────────────────────┘
                                 │ USB / UART
┌────────────────────────────────▼───────────────────────────┐
│               Arduino Mega 2560 (gauge_V4)                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────┐  │
│  │  EEPROM  │  │  CAN bus │  │  Motors  │  │  Displays │  │
│  │  Store   │  │  (500k)  │  │  (x5)    │  │  (2×OLED) │  │
│  └──────────┘  └──────────┘  └──────────┘  └───────────┘  │
└────────────────────────────────────────────────────────────┘
```

### Data Flow

```
READ:   PC sends "get <param>"  →  Arduino responds "<param> <value>"
WRITE:  PC sends "set <param> <value>"  →  Arduino echoes "ok" or "err"
SAVE:   PC sends "save"  →  Arduino writes all dirty values to EEPROM
```

---

## 2. Memory Requirements

### EEPROM Memory Map

The Arduino Mega 2560 provides **4096 bytes** of EEPROM. The current firmware uses the following layout:

| Address | Size (bytes) | Variable | Description |
|---------|-------------|----------|-------------|
| 0–3 | 4 | `dispArray1[0..3]` | Display 1 menu state (4 × uint8) |
| 4 | 1 | `dispArray2[0]` | Display 2 screen selection (uint8) |
| 5 | 1 | `clockOffset` | Time zone offset from UTC (int8, −12 to +12) |
| 6–9 | 4 | `odo` | Total odometer in km (float) |
| 10–13 | 4 | `odoTrip` | Trip odometer in km (float) |
| 14–17 | 4 | `fuelSensorRaw` | Last fuel sensor ADC reading (int, persists level across restarts) |
| 18 | 1 | `units` | Unit system: 0 = metric, 1 = imperial |
| **19–511** | **493** | *(reserved)* | Available for configuration tool expansion |
| **512–1023** | **512** | *(reserved)* | Custom splash image 1 (128×32 = 512 bytes) |
| **1024–1535** | **512** | *(reserved)* | Custom splash image 2 (128×32 = 512 bytes) |
| 1536–4095 | 2560 | *(free)* | Available for future parameters |

> **Note:** The ranges marked *reserved* are not yet implemented in firmware. They represent the allocation plan for the configuration tool (v1.2).

### SRAM Analysis

The Arduino Mega 2560 has **8 KB of SRAM**. Key consumers:

| Item | Size | Notes |
|------|------|-------|
| `leds[]` (MAX_LEDS=64) | 192 bytes | CRGB = 3 bytes/LED |
| Two OLED framebuffers | 2 × 512 = 1024 bytes | SSD1306 internal buffer |
| SwitecX12 motor objects (×5) | ~100 bytes | Step tables, state |
| Global variables (globals.cpp) | ~300 bytes | Sensor readings, CAN buffers |
| Stack (estimated) | ~512 bytes | Function call depth |
| **Total estimated** | **~2128 bytes** | **~26% of 8 KB** |

**Budget constraint:** Any new feature must not consume more than ~1 KB of additional SRAM. Lookup tables and constant data must use `PROGMEM`.

### Flash Usage

The Arduino Mega 2560 has **256 KB** of flash. All `PROGMEM` bitmaps, lookup tables, and string literals count against this budget. Each 128×32 custom splash image is 512 bytes of PROGMEM (or EEPROM if stored there).

### Memory Optimization Strategies

- Store lookup table X-values and Y-values in `PROGMEM` arrays (`const uint16_t ... PROGMEM`).
- Use `EEPROM.update()` (not `write()`) to reduce write cycles on unchanged values.
- Prefer `uint8_t`/`int8_t` over `int` wherever range permits.
- Use `F()` macro for all Serial string literals to keep strings in flash.

---

## 3. Serial Communication Protocol

### Physical Layer

| Parameter | Value |
|-----------|-------|
| Baud rate | 115200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Flow control | None |
| Line ending | `\n` or `\r\n` |

### Command Format

```
<verb> [<param>] [<value>]\n
```

- All fields are ASCII, space-delimited.
- Commands are case-sensitive (lowercase by convention).
- Responses are terminated with `\n`.

### Complete Command Reference

#### Runtime Signal Injection (already implemented)

| Command | Description | Example |
|---------|-------------|---------|
| `spd <kph>` | Set speed (km/h); stored as km/h × 100. Requires `SPEED_SOURCE=6`. | `spd 80` |
| `rpm <value>` | Set engine RPM. Requires `RPM_SOURCE=4`. | `rpm 3500` |
| `odo motor <N>` | Rotate odometer motor N revolutions; rejected if `spd ≠ 0`. | `odo motor 5` |

#### Configuration Read/Write (proposed for PC tool)

| Command | Response | Description |
|---------|----------|-------------|
| `get <param>` | `<param> <value>` | Read one parameter |
| `set <param> <value>` | `ok` or `err: <reason>` | Write one parameter (RAM only until `save`) |
| `save` | `ok` | Flush all changed values to EEPROM |
| `load` | `ok` | Re-read all values from EEPROM (discard RAM changes) |
| `dump` | `<param> <value>\n` × N, then `end` | Dump all parameters |
| `reset` | `ok` | Reset all parameters to firmware defaults |
| `ping` | `pong` | Check connectivity |
| `version` | `version <major>.<minor>` | Firmware version string |

#### Splash Screen Upload (v1.2)

| Command | Response | Description |
|---------|----------|-------------|
| `splash begin <id>` | `ready` | Start upload for splash slot 1 or 2 |
| `splash data <hex32>` | `ok` or `err` | Send 16 bytes as 32 hex chars |
| `splash end <crc32>` | `ok` or `err: crc` | Finalize and verify with CRC32 |
| `splash test <id>` | `ok` | Display uploaded splash immediately |
| `splash clear <id>` | `ok` | Erase splash slot (restore default) |

### Parameter Names

| Name | Type | Range | Description |
|------|------|-------|-------------|
| `m1_sweep` | uint16 | 100–4095 | Motor 1 full sweep steps |
| `m2_sweep` | uint16 | 100–4095 | Motor 2 full sweep steps |
| `m3_sweep` | uint16 | 100–4095 | Motor 3 full sweep steps |
| `m4_sweep` | uint16 | 100–4095 | Motor 4 full sweep steps |
| `ms_sweep` | uint16 | 100–8000 | Motor S (speedometer) full sweep steps |
| `ms_zero_delay` | uint16 | 10–5000 | Motor S zeroing step delay (µs) |
| `ms_zero_factor` | float | 0.1–1.0 | Motor S zeroing sweep fraction |
| `motor_sweep_ms` | uint16 | 100–5000 | Startup test sweep duration (ms) |
| `filter_vbatt` | uint8 | 1–64 | Battery voltage filter coefficient |
| `vbatt_scaler` | float | 0.001–0.1 | Voltage divider scale factor |
| `filter_fuel` | uint8 | 1–64 | Fuel sensor filter coefficient |
| `filter_therm` | uint8 | 1–100 | Thermistor filter coefficient |
| `filter_av1` | uint8 | 1–16 | AV1 sensor filter coefficient |
| `filter_av2` | uint8 | 1–16 | AV2 sensor filter coefficient |
| `filter_av3` | uint8 | 1–16 | AV3 sensor filter coefficient |
| `revs_per_km` | uint16 | 100–10000 | VSS pulses per km |
| `teeth_per_rev` | uint8 | 1–64 | VSS teeth per shaft revolution |
| `filter_hall` | uint8 | 1–255 | Hall speed EMA coefficient |
| `hall_speed_min` | uint8 | 0–100 | Min reportable speed (km/h×100) |
| `cyl_count` | uint8 | 2–16 | Engine cylinder count |
| `filter_rpm` | uint8 | 1–255 | RPM EMA filter coefficient |
| `rpm_debounce_us` | uint16 | 100–20000 | RPM pulse debounce window (µs) |
| `engine_rpm_min` | uint8 | 0–255 | Min reportable RPM |
| `speedo_max` | uint16 | 1000–30000 | Max speedometer (mph×100) |
| `num_leds` | uint8 | 1–64 | LED strip length |
| `warn_leds` | uint8 | 0–32 | Warning zone LED count |
| `shift_leds` | uint8 | 0–16 | Shift light LED count |
| `tach_max` | uint16 | 1000–15000 | Shift RPM |
| `tach_min` | uint16 | 0–5000 | Min display RPM |
| `odo_steps` | uint16 | 512–8192 | Odometer motor steps/rev |
| `odo_motor_teeth` | uint8 | 1–64 | Odometer motor gear teeth |
| `odo_gear_teeth` | uint8 | 1–64 | Odometer driven gear teeth |
| `speed_source` | uint8 | 0–6 | Speed signal source |
| `rpm_source` | uint8 | 0–4 | RPM signal source |
| `oil_prs_source` | uint8 | 0–5 | Oil pressure source |
| `fuel_prs_source` | uint8 | 0–5 | Fuel pressure source |
| `coolant_src` | uint8 | 0–3 | Coolant temp source |
| `oil_temp_src` | uint8 | 0–2 | Oil temp source |
| `map_source` | uint8 | 0–5 | MAP / boost source |
| `lambda_source` | uint8 | 0–4 | Lambda / AFR source |
| `fuel_lvl_src` | uint8 | 0–2 | Fuel level source |
| `oil_warn_kpa` | float | 0–1000 | Oil pressure warning threshold (kPa gauge) |
| `coolant_warn_c` | float | 50–200 | Coolant temp warning threshold (°C) |
| `batt_warn_v` | float | 5–16 | Battery voltage warning threshold (V) |
| `engine_run_rpm` | int | 0–2000 | Min RPM to consider engine running |
| `fuel_warn_pct` | uint8 | 0–50 | Low fuel warning threshold (%) |
| `clock_offset` | int8 | −12–12 | Time zone offset from UTC (hours) |
| `fuel_capacity` | float | 1–200 | Fuel tank capacity (gallons) |
| `can_protocol` | uint8 | 0–3 | CAN protocol: 0=Haltech v2, 1=Megasquirt, 2=AiM, 3=OBDII |
| `units` | uint8 | 0–1 | 0 = metric, 1 = imperial |

### Error Codes

| Code | Meaning |
|------|---------|
| `err: unknown` | Parameter name not recognized |
| `err: range` | Value outside allowed min/max |
| `err: type` | Value could not be parsed as the required type |
| `err: busy` | Command rejected; previous operation in progress |
| `err: crc` | CRC32 mismatch during splash upload |

### Example Command Sequences

**Read all motor sweeps:**
```
dump
m1_sweep 696
m2_sweep 696
m3_sweep 696
m4_sweep 696
ms_sweep 4032
...
end
```

**Update speedometer max and save:**
```
set speedo_max 12000
ok
save
ok
```

**Upload a custom splash image (slot 1):**
```
splash begin 1
ready
splash data 0000000000000000ffffffffffffffff
ok
splash data ...   ← repeat for all 32 chunks (512 bytes / 16 bytes per chunk)
ok
splash end a3f29b1c
ok
splash test 1
ok
```

---

## 4. Data Structures

### Arduino C/C++ Structures

```cpp
// ===== EEPROM LAYOUT =====
// Byte addresses for all persisted values
struct EEPROMLayout {
    // addresses 0–3
    uint8_t  dispArray1[4];        // Display 1 menu state
    // address 4
    uint8_t  dispArray2;           // Display 2 screen selection
    // address 5
    uint8_t  clockOffset;          // UTC offset (treated as signed, 0–23 wraps)
    // addresses 6–9
    float    odo;                  // Total odometer (km)
    // addresses 10–13
    float    odoTrip;              // Trip odometer (km)
    // addresses 14–17
    int      fuelSensorRaw;        // Fuel sensor ADC snapshot
    // address 18
    uint8_t  units;                // 0=metric, 1=imperial
    // addresses 512–1023
    uint8_t  customSplash1[512];   // User splash image slot 1
    // addresses 1024–1535
    uint8_t  customSplash2[512];   // User splash image slot 2
};
```

```cpp
// ===== MOTOR CONFIGURATION =====
struct MotorConfig {
    uint16_t sweep;         // Full sweep in steps
    uint8_t  stepPin;       // Step pulse pin
    uint8_t  dirPin;        // Direction pin
    uint8_t  source;        // MOTOR_SRC_* enum
    // 3-point piecewise mapping (v1.1)
    int      inLow;         // Input at needle minimum
    int      inMid;         // Input at needle midpoint
    int      inHigh;        // Input at needle maximum
};
```

```cpp
// ===== LOOKUP CURVE (unified, v1.1) =====
// All curves use 2–8 breakpoints on a unified voltage scale.
struct Curve {
    uint8_t  numPoints;             // 2–8
    uint16_t x[8];                  // X breakpoints (millivolts or sensor units)
    int16_t  y[8];                  // Y values (physical units × scale)
};
```

### Python Dataclasses (v1.1+)

```python
from dataclasses import dataclass, field
from typing import List

@dataclass
class MotorConfig:
    sweep: int = 696
    source: int = 0          # MOTOR_SRC_NONE
    in_low: int = 0
    in_mid: int = 0
    in_high: int = 0

@dataclass
class Curve:
    num_points: int = 2
    x: List[int] = field(default_factory=lambda: [0, 5000, 0, 0, 0, 0, 0, 0])
    y: List[int] = field(default_factory=lambda: [0, 1000, 0, 0, 0, 0, 0, 0])

@dataclass
class GaugeConfig:
    # Motors
    motor1: MotorConfig = field(default_factory=MotorConfig)
    motor2: MotorConfig = field(default_factory=MotorConfig)
    motor3: MotorConfig = field(default_factory=MotorConfig)
    motor4: MotorConfig = field(default_factory=MotorConfig)
    motor_s: MotorConfig = field(default_factory=MotorConfig)

    # Filters (0–255 unified scale, v1.1)
    filter_vbatt: int = 8
    filter_fuel: int = 1
    filter_therm: int = 50
    filter_av1: int = 4
    filter_av2: int = 12
    filter_av3: int = 12
    filter_hall: int = 64
    filter_rpm: int = 179

    # Curves
    therm_curve: Curve = field(default_factory=Curve)
    fuel_curve: Curve = field(default_factory=Curve)

    # System
    units: int = 0            # 0=metric, 1=imperial
    can_protocol: int = 0     # 0=Haltech v2
    clock_offset: int = 0
    fuel_capacity: float = 16.0

    # Splash (v1.2)
    custom_splash1: bytes = field(default_factory=lambda: bytes(512))
    custom_splash2: bytes = field(default_factory=lambda: bytes(512))
```

### Enumerations

```python
from enum import IntEnum

class MotorSource(IntEnum):
    NONE       = 0   # Motor disabled; skip calculations
    FUEL_LVL   = 1   # Fuel level (gallons or liters)
    COOLANT    = 2   # Coolant temperature
    OIL_TEMP   = 3   # Oil temperature
    OIL_PRS    = 4   # Oil pressure
    FUEL_PRS   = 5   # Fuel pressure
    SPEED      = 6   # Vehicle speed
    RPM        = 7   # Engine RPM
    BATT_VOLT  = 8   # Battery voltage
    BOOST      = 9   # Manifold pressure / boost

class SpeedSource(IntEnum):
    OFF        = 0
    CAN        = 1
    HALL       = 2
    GPS        = 3
    SYNTHETIC  = 4   # Debug / demo
    ODO_TEST   = 5   # 1-mile deterministic profile
    SERIAL     = 6   # Set via serial command

class CANProtocol(IntEnum):
    HALTECH_V2  = 0
    MEGASQUIRT  = 1
    AIM         = 2
    OBDII       = 3

class UnitSystem(IntEnum):
    METRIC   = 0
    IMPERIAL = 1

class SplashID(IntEnum):
    SLOT_1 = 1
    SLOT_2 = 2
```

---

## 5. Configuration Parameters

### Motor Configuration (5 motors)

| Parameter | Default | Min | Max | Notes |
|-----------|---------|-----|-----|-------|
| M1_SWEEP | 696 | 100 | 4095 | 58° × 12 steps/° |
| M2_SWEEP | 696 | 100 | 4095 | |
| M3_SWEEP | 696 | 100 | 4095 | |
| M4_SWEEP | 696 | 100 | 4095 | |
| MS_SWEEP | 4032 | 100 | 8000 | Speedometer NEMA14; (118°/0.9°) × 32 microsteps |
| MS_ZERO_STEP_DELAY_US | 40 | 10 | 5000 | µs/step during zeroing |
| MS_ZERO_SWEEP_FACTOR | 0.25 | 0.1 | 1.0 | Fraction of MS_SWEEP for zeroing |
| MOTOR_SWEEP_TIME_MS | 1000 | 100 | 5000 | Startup test duration (ms) |

### Scalar Parameters

| Parameter | Default | Min | Max | Notes |
|-----------|---------|-----|-----|-------|
| VBATT_SCALER | 0.040923 | 0.001 | 0.1 | (5.0/1023) × (R1+R2)/R2; R1=10k, R2=3.3k |
| REVS_PER_KM | 1625 | 100 | 10000 | VSS shaft revolutions per km |
| TEETH_PER_REV | 8 | 1 | 64 | VSS teeth per shaft revolution |
| HALL_SPEED_MIN | 50 | 0 | 100 | km/h × 100; 50 = 0.5 km/h dead zone |
| RPM_DEBOUNCE_MICROS | 5000 | 100 | 20000 | µs; reject coil-ringdown echoes |
| ENGINE_RPM_MIN | 100 | 0 | 255 | Minimum displayed RPM |
| CYL_COUNT | 8 | 2 | 16 | Engine cylinders |
| SPEEDO_MAX | 10000 | 1000 | 30000 | mph × 100; 10000 = 100 mph |
| NUM_LEDS | 27 | 1 | 64 | Total LED strip count |
| WARN_LEDS | 6 | 0 | 32 | Warning zone per side |
| SHIFT_LEDS | 2 | 0 | 16 | Shift-light zone per side |
| TACH_MAX | 6000 | 1000 | 15000 | Shift RPM |
| TACH_MIN | 3000 | 0 | 5000 | Min lit RPM |
| ODO_STEPS | 2048 | 512 | 8192 | Steps per revolution (wave drive) |
| ODO_MOTOR_TEETH | 16 | 1 | 64 | Motor gear tooth count |
| ODO_GEAR_TEETH | 20 | 1 | 64 | Driven gear tooth count |
| OIL_PRS_WARN_THRESHOLD | 60.0 | 0 | 1000 | kPa gauge; warn below this |
| COOLANT_TEMP_WARN_THRESHOLD | 110.0 | 50 | 200 | °C; warn above this |
| BATT_VOLT_WARN_THRESHOLD | 11.0 | 5 | 16 | V; warn below this |
| ENGINE_RUNNING_RPM_MIN | 400 | 0 | 2000 | RPM threshold for fault logic |
| FUEL_LVL_WARN_THRESHOLD_PCT | 5 | 0 | 50 | %; warn below this |
| CLOCK_OFFSET | 0 | −12 | 12 | UTC offset in hours |
| FUEL_CAPACITY | 16.0 | 1 | 200 | Tank size in gallons |

### Lookup Curves

#### Thermistor Temperature Curve (default: GM sensor)

| Index | Voltage (mV) | Temperature (°C) |
|-------|-------------|-----------------|
| 0 | 230 | 150 |
| 1 | 670 | 105 |
| 2 | 1430 | 75 |
| 3 | 3700 | 25 |
| 4 | 4630 | −5 |
| 5 | 4950 | −40 |

#### Fuel Level Curve (default: vehicle-specific float sender)

| Index | Voltage (mV) | Gallons × 10 |
|-------|-------------|--------------|
| 0 | 870 | 160 |
| 1 | 1030 | 140 |
| 2 | 1210 | 120 |
| 3 | 1400 | 100 |
| 4 | 1600 | 80 |
| 5 | 1970 | 60 |
| 6 | 2210 | 40 |
| 7 | 2250 | 20 |
| 8 | 2300 | 0 |

### Signal Source Selection

| Parameter | Default | Options |
|-----------|---------|---------|
| SPEED_SOURCE | 2 (Hall) | 0=off, 1=CAN, 2=Hall, 3=GPS, 4=Synthetic, 5=Odo test, 6=Serial |
| RPM_SOURCE | 2 (coil) | 0=off, 1=CAN, 2=coil negative, 3=Synthetic, 4=Serial |
| OIL_PRS_SOURCE | 5 (synthetic) | 0=off, 1=CAN, 2=AV1, 3=AV2, 4=AV3, 5=Synthetic |
| FUEL_PRS_SOURCE | 5 (synthetic) | 0=off, 1=CAN, 2=AV1, 3=AV2, 4=AV3, 5=Synthetic |
| COOLANT_TEMP_SOURCE | 3 (synthetic) | 0=off, 1=CAN, 2=thermistor, 3=Synthetic |
| OIL_TEMP_SOURCE | 2 (therm) | 0=off, 1=CAN, 2=thermistor |
| MAP_SOURCE | 5 (synthetic) | 0=off, 1=CAN, 2=AV1, 3=AV2, 4=AV3, 5=Synthetic |
| LAMBDA_SOURCE | 1 (CAN) | 0=off, 1=CAN, 2=AV1, 3=AV2, 4=AV3 |
| FUEL_LVL_SOURCE | 2 (synthetic) | 0=off, 1=analog sensor, 2=Synthetic |

### Display Screen Configuration

#### Display 1 Screen IDs

| `dispArray1[0]` | Screen |
|-----------------|--------|
| 0 | Settings menu |
| 1 | Oil Pressure (icon + value) |
| 2 | Coolant Temperature (icon + value) |
| 3 | Fuel Level (icon + value) |
| 4 | Battery Voltage (icon + value) |
| 5 | Engine RPM |
| 6 | Vehicle Speed |
| 7 | Air/Fuel Ratio |
| 8 | Fuel Pressure |
| 9 | Boost / MAP (bar graph + icon) |
| 10 | Boost / MAP (text + icon) |
| 11 | Oil Temperature |
| 12 | Fuel Composition (ethanol %) |
| 13 | Injector Duty Cycle |
| 14 | Ignition Timing |
| 15 | Trip Odometer (with reset submenu) |
| 16 | Clock (GPS time + offset) |
| 17 | Falcon Script logo (splash) |

#### Display 2 Screen IDs

| `dispArray2[0]` | Screen |
|-----------------|--------|
| 0 | Oil Pressure |
| 1 | Coolant Temperature |
| 2 | Fuel Level |
| 3 | Battery Voltage |
| 4 | Engine RPM |
| 5 | Vehicle Speed |
| 6 | Boost / MAP (bar graph) |
| 7 | Boost / MAP (text) |
| 8 | Clock |
| 9 | Falcon Script logo |

### Output Enable Flags (v1.1)

| Flag | Description |
|------|-------------|
| `motor1_enable` | Enable/disable Motor 1 calculations and stepping |
| `motor2_enable` | Enable/disable Motor 2 |
| `motor3_enable` | Enable/disable Motor 3 |
| `motor4_enable` | Enable/disable Motor 4 |
| `motorS_enable` | Enable/disable speedometer motor |
| `tach_enable` | Enable/disable LED tachometer strip |
| `display1_enable` | Enable/disable Display 1 |
| `display2_enable` | Enable/disable Display 2 |
| `can_tx_enable` | Enable/disable CAN bus transmission |
| `odo_motor_enable` | Enable/disable mechanical odometer motor |

---

## 6. Calibration Curves

### Unified Curve Editor Specification (v1.1)

All sensor curves share a single format: **2 to 8 breakpoints**, where each breakpoint is an (X, Y) pair:
- **X:** Input value in millivolts (0–5000 mV) or raw sensor units
- **Y:** Output physical value (temperature in °C, pressure in kPa, fuel in gallons × 10, etc.)

Interpolation is **piecewise linear** between adjacent points. Values outside the defined range are **clamped** to the first/last Y value.

```
Y
|
|      *──────*
|   *              *
| *                   *
*                       *
+─────────────────────── X (mV)
```

### Voltage Divider Calculations

For resistive sensors connected through a voltage divider:

```
                +5V
                 │
                R1
                 │
                 ├─── ADC input (0–5V)
                 │
            R_sensor (varies with measured quantity)
                 │
                GND
```

**Voltage at ADC:**
```
V_adc = 5.0 × R_sensor / (R1 + R_sensor)
```

**ADC reading (10-bit, 0–1023):**
```
ADC = V_adc × 1023 / 5.0
```

**Millivolts for curve lookup:**
```
mV = V_adc × 1000
```

**Battery voltage divider (R1=10kΩ, R2=3.3kΩ):**
```
VBATT_SCALER = (5.0 / 1023) × (10000 + 3300) / 3300 = 0.040923
V_batt = ADC_reading × VBATT_SCALER
```

### Resistance-to-Voltage Conversion

For GM-style thermistors (NTC, R_ref = 2490 Ω pull-up to +5V):

```
V_adc = 5.0 × R_thermistor / (2490 + R_thermistor)
```

At 80°C (typical GM sensor ≈ 330 Ω):
```
V = 5.0 × 330 / (2490 + 330) = 0.586 V → 586 mV
```

### Preset Curves

#### GM Coolant Temperature Sender (Typical)

| Resistance (Ω) | Temperature (°C) | Voltage at ADC (mV) |
|----------------|-----------------|---------------------|
| 3520 | 25 | 3700 |
| 1188 | 50 | 2520 |
| 467 | 75 | 1430 |
| 198 | 100 | 670 |

#### Standard Fuel Level Sender (0–90 Ω)

| Resistance (Ω) | Level | Voltage (mV) |
|----------------|-------|--------------|
| 0 (full) | 100% | ~870 |
| 45 (half) | 50% | ~1600 |
| 90 (empty) | 0% | ~2300 |

### `curveLookup` Algorithm

```cpp
/**
 * curveLookup - Piecewise linear interpolation from lookup table
 *
 * @param input    - Input value (mV or raw units × scale)
 * @param table_x  - X breakpoints array (PROGMEM)
 * @param table_y  - Y values array (PROGMEM)
 * @param length   - Number of breakpoints (2–8)
 * @return Interpolated Y value
 */
int16_t curveLookup(uint16_t input, const uint16_t *table_x,
                    const int16_t *table_y, uint8_t length) {
    if (input <= table_x[0])        return table_y[0];
    if (input >= table_x[length-1]) return table_y[length-1];
    for (uint8_t i = 0; i < length - 1; i++) {
        if (input >= table_x[i] && input < table_x[i+1]) {
            long num = (long)(input - table_x[i]) * (table_y[i+1] - table_y[i]);
            long den = table_x[i+1] - table_x[i];
            return table_y[i] + (int16_t)(num / den);
        }
    }
    return table_y[length-1];
}
```

---

## 7. Motor Assignment System

### Dynamic Motor Assignment (v1.1)

Each of the five motors can be independently assigned to any signal source. When `MOTOR_SRC_NONE` is selected, all angle calculations for that motor are skipped — saving CPU time and preventing twitching on unused gauges.

### Source Enumeration

| Value | Source | Unit |
|-------|--------|------|
| 0 | NONE (disabled) | — |
| 1 | Fuel Level | gallons or liters |
| 2 | Coolant Temperature | °C or °F |
| 3 | Oil Temperature | °C or °F |
| 4 | Oil Pressure | kPa or PSI |
| 5 | Fuel Pressure | kPa or PSI |
| 6 | Vehicle Speed | km/h or mph |
| 7 | Engine RPM | RPM |
| 8 | Battery Voltage | V |
| 9 | Boost / MAP | kPa or PSI |

### 3-Point Piecewise Mapping (v1.1)

Rather than a simple 2-point `map()`, each motor supports a **midpoint calibration** to correct for non-linear gauge scales:

```
angle
  │                      * sweep-1
  │              *
  │      *
  0──────────────────── input
     inLow  inMid  inHigh
```

```cpp
int motorAngle3pt(int input, int inLow, int inMid, int inHigh,
                  int sweep) {
    int angle;
    if (input <= inMid) {
        angle = map(input, inLow, inMid, 1, sweep / 2);
    } else {
        angle = map(input, inMid, inHigh, sweep / 2, sweep - 1);
    }
    return constrain(angle, 1, sweep - 1);
}
```

### Coolant Temperature Mapping (Current Implementation)

The coolant temperature gauge uses a built-in 3-point map to compress the cold range and expand the hot warning range:

```cpp
int coolantTempAngle(int sweep) {
    int angle;
    if (coolantTemp < 90) {
        angle = map((long)coolantTemp, 50, 90, 1, sweep / 2);
    } else {
        angle = map((long)coolantTemp, 90, 115, sweep / 2, sweep - 1);
    }
    return constrain(angle, 1, sweep - 1);
}
```

### Speedometer Angle Algorithm

```cpp
/**
 * speedometerAngleS - Integer-math speedometer angle
 * Input: spd (km/h × 100), SPEEDO_MAX (mph × 100)
 * Output: motor steps (1 to sweep-1)
 */
int speedometerAngleS(int sweep) {
    int local_spd = constrain(spd, 0, 30000);
    // km/h*100 → mph*100 using integer math (×62137/100000)
    long spd_mph = ((long)local_spd * 62137L) / 100000L;
    if (spd_mph < 50) spd_mph = 0;              // dead zone < 0.5 mph
    if (spd_mph > SPEEDO_MAX) spd_mph = SPEEDO_MAX;
    int angle = ((long)spd_mph * (long)(sweep - 2)) / (long)SPEEDO_MAX + 1;
    return constrain(angle, 1, sweep - 1);
}
```

---

## 8. Filter System

### Unified 0–255 Scale (v1.1)

All filter coefficients use a **0–255 integer scale** where:
- `255` = no filtering (pass-through)
- `128` = moderate filtering (~50% weight on new sample)
- `1` = maximum filtering (almost no response to new data)

This replaces the previous ad-hoc scales (e.g., "out of 64", "out of 100") with a consistent convention.

### Exponential Moving Average (EMA) Implementation

```cpp
// readSensor - Apply EMA filter to analog reading
// alpha is in [1..255]; output = (alpha/256)*new + (1 - alpha/256)*prev
float readSensor(uint8_t pin, float prev, uint8_t alpha) {
    float raw = analogRead(pin) * (5000.0f / 1023.0f);  // mV
    return prev + ((float)alpha / 256.0f) * (raw - prev);
}
```

### Conversion from Old Scales

| Old scale | Old value | Formula | New value (0–255) |
|-----------|-----------|---------|-------------------|
| Out of 64 | 8 | `round(8/64 × 255)` | 32 |
| Out of 100 | 50 | `round(50/100 × 255)` | 128 |
| EMA 0.8 | 205 | `round(0.8 × 256)` | 205 |
| EMA 0.7 | 179 | `round(0.7 × 256)` | 179 |

### Default Filter Values

| Parameter | Default (0–255) | Effective Weight on New Sample |
|-----------|----------------|-------------------------------|
| FILTER_VBATT | 32 | 12.5% |
| FILTER_FUEL | 4 | 1.6% |
| FILTER_THERM | 128 | 50% |
| FILTER_AV1 | 64 | 25% |
| FILTER_AV2 | 48 | 18.8% |
| FILTER_AV3 | 48 | 18.8% |
| FILTER_HALL_SPEED | 64 | 25% |
| FILTER_ENGINE_RPM | 179 | 70% |

---

## 9. Display Screen Configuration

### Screen Rotation System

Display 1 wraps through 18 screens (indices 0–17) using the rotary encoder. The encoder ISR updates `dispArray1[menuLevel]`, clamping at `nMenuLevel` and wrapping at both ends:

```
0 (Settings) ↔ 1 ↔ 2 ↔ … ↔ 17 (Falcon logo)
     ↑                              ↓
     └──────────── wrap ────────────┘
```

`nMenuLevel = 17` means the maximum valid index is 17, giving 18 positions.

### Enable/Disable Functionality (v1.1)

The configuration tool can hide screens from the rotation sequence by marking them disabled. The firmware skips disabled screen indices, so the encoder moves directly to the next enabled screen.

```cpp
// Proposed implementation in firmware
uint32_t screenEnableMask1 = 0x3FFFF;  // Bitmask: bit N = screen N enabled (18 screens)
uint16_t screenEnableMask2 = 0x3FF;    // Bitmask: bit N = screen N enabled (10 screens)
```

### Position Ordering (v1.1)

The configuration tool can specify a custom ordering for Display 1 screens using a permutation array stored in EEPROM. The default order (0–17) maps index to screen ID directly.

### Menu Hierarchy

```
Level 0: Rotate to select screen (0–17)
Level 1: Enter submenu on button press (e.g., Settings, Trip Odo reset)
Level 2: Sub-submenu (e.g., Display 2 selection, Units, Clock offset)
```

The array `dispArray1[4]` tracks position at each level:
- `[0]` = current level-0 screen
- `[1]` = level-1 selection within that screen
- `[2]` = level-2 selection
- `[3]` = level-3 (currently unused)

### Dirty Tracking for Display Updates

Each display function compares current values against `*_prev` variables. The display is only redrawn when a value changes beyond a threshold, reducing SPI bus traffic.

Force-update is triggered by setting `dispArray1_prev[0] = 255` (an invalid screen index), which guarantees a full redraw on the next frame.

---

## 10. Custom Splash Screen Feature (v1.2)

### EEPROM Storage Allocation

| Slot | Start Address | Size | Notes |
|------|--------------|------|-------|
| Custom Splash 1 | 512 | 512 bytes | User image |
| Custom Splash 2 | 1024 | 512 bytes | User image |
| Slot validity flag 1 | 509 | 1 byte | 0xA5 = valid, else use default |
| Slot validity flag 2 | 510 | 1 byte | 0xA5 = valid, else use default |

### SSD1306 Bitmap Format

The SSD1306 uses **vertical byte addressing** for its 128×32 display:
- 128 columns × 4 pages (each page = 8 pixel rows)
- Total: 512 bytes
- Byte at `[page * 128 + col]` controls 8 vertical pixels in that column

```
Byte layout (one column):
Bit 7 = row 7 (bottom of page)
Bit 6 = row 6
...
Bit 0 = row 0 (top of page)

Memory address: page × 128 + column
Pages: 0 (rows 0–7), 1 (rows 8–15), 2 (rows 16–23), 3 (rows 24–31)
```

### Image Conversion Process (PIL/Pillow)

```python
from PIL import Image
import struct, zlib

class SSD1306Converter:
    """Convert any image to 128×32 SSD1306-format bytes."""

    def convert(self, path: str) -> bytes:
        img = Image.open(path).convert("1")          # Force monochrome
        img = img.resize((128, 32), Image.LANCZOS)   # Resize to display
        buf = bytearray(512)
        for page in range(4):
            for col in range(128):
                byte = 0
                for bit in range(8):
                    row = page * 8 + bit
                    px = img.getpixel((col, row))
                    if px:
                        byte |= (1 << bit)
                buf[page * 128 + col] = byte
        return bytes(buf)

    def preview(self, data: bytes) -> Image.Image:
        """Reconstruct PIL image from SSD1306 bytes for GUI preview."""
        img = Image.new("1", (128, 32))
        for page in range(4):
            for col in range(128):
                byte = data[page * 128 + col]
                for bit in range(8):
                    row = page * 8 + bit
                    img.putpixel((col, row), (byte >> bit) & 1)
        return img
```

### Upload Protocol

1. PC sends `splash begin <id>` → Arduino allocates write buffer, responds `ready`.
2. PC sends image in 32-byte chunks using `splash data <hex64>` (64 hex chars = 32 bytes). Arduino writes each chunk to EEPROM and responds `ok`.
3. PC sends `splash end <crc32_hex>` with CRC32 of the full 512 bytes. Arduino verifies and responds `ok` or `err: crc`.
4. On `ok`, Arduino sets the slot validity flag (0xA5).

### CRC32 Implementation

```python
import zlib

def crc32_of(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

def crc32_hex(data: bytes) -> str:
    return f"{crc32_of(data):08x}"
```

Arduino-side:
```cpp
uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}
```

### Arduino Splash Display Functions

```cpp
/**
 * dispCustomSplash - Display a user-uploaded splash from EEPROM
 *
 * @param display  - Pointer to Adafruit_SSD1306 instance
 * @param slot     - 1 or 2
 */
void dispCustomSplash(Adafruit_SSD1306 *display, uint8_t slot) {
    uint16_t addr = (slot == 1) ? 512 : 1024;
    uint8_t buf[512];
    for (int i = 0; i < 512; i++) {
        buf[i] = EEPROM.read(addr + i);
    }
    display->clearDisplay();
    display->drawBitmap(0, 0, buf, 128, 32, WHITE);
    display->display();
}

/**
 * isSplashValid - Check if a splash slot contains a valid image
 *
 * @param slot  - 1 or 2
 * @return true if slot has been uploaded
 */
bool isSplashValid(uint8_t slot) {
    uint16_t flagAddr = (slot == 1) ? 509 : 510;
    return EEPROM.read(flagAddr) == 0xA5;
}
```

### Python Serial Uploader

```python
import serial, time, zlib

class SplashUploader:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 2.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(2)  # Wait for Arduino reset

    def _cmd(self, line: str) -> str:
        self.ser.write((line + '\n').encode())
        return self.ser.readline().decode().strip()

    def upload(self, slot: int, data: bytes) -> bool:
        assert len(data) == 512
        crc = zlib.crc32(data) & 0xFFFFFFFF

        resp = self._cmd(f"splash begin {slot}")
        if resp != "ready":
            raise RuntimeError(f"begin failed: {resp}")

        for offset in range(0, 512, 32):
            chunk = data[offset:offset+32]
            hex_str = chunk.hex()
            resp = self._cmd(f"splash data {hex_str}")
            if resp != "ok":
                raise RuntimeError(f"data chunk failed at offset {offset}: {resp}")

        resp = self._cmd(f"splash end {crc:08x}")
        return resp == "ok"

    def test(self, slot: int) -> bool:
        return self._cmd(f"splash test {slot}") == "ok"

    def close(self):
        self.ser.close()
```

---

## 11. Arduino Implementation Guide

### File Structure

```
gauge_V4/
├── gauge_V4.ino          ← Main sketch: setup(), loop(), Timer3 ISR
├── config_hardware.h     ← constexpr pin assignments, timing constants
├── config_calibration.h  ← Calibration parameter declarations (extern)
├── config_calibration.cpp← Calibration parameter definitions (defaults)
├── globals.h             ← Global variable declarations (extern)
├── globals.cpp           ← Global variable definitions, EEPROM addresses
├── can.h / can.cpp       ← CAN bus send/receive/parse (Haltech, MS, AiM, OBDII)
├── display.h / display.cpp← All display functions, menu system
├── menu.h / menu.cpp     ← Encoder ISR, swRead(), goToLevel0()
├── gps.h / gps.cpp       ← GPS parsing, odometer update
├── sensors.h / sensors.cpp← sigSelect(), analog reads, Hall, RPM
├── outputs.h / outputs.cpp← Motor angle functions, LED tach, odometer motor
├── utilities.h / utilities.cpp← shutdown(), synthetic generators, serial commands
└── image_data.h / image_data.cpp← PROGMEM bitmaps for icons and logos
```

### Files to Create (New)

| File | Purpose |
|------|---------|
| `serial_config.h/cpp` | Implement `get`/`set`/`save`/`dump`/`load` command handler |
| `splash.h/cpp` | Implement splash upload protocol, EEPROM read/write, CRC32 |

### Files to Modify (Existing)

| File | Change |
|------|--------|
| `utilities.cpp` → `processSerialCommands()` | Extend to dispatch to `serial_config.cpp` handlers |
| `utilities.cpp` → `startup()` | Call `dispCustomSplash()` if slot is valid |
| `globals.cpp` | Add EEPROM addresses for splash slots and validity flags |
| `config_calibration.h/.cpp` | Add enable flags, ordering arrays (v1.1) |

### Serial Command Parser Integration

```cpp
// In utilities.cpp, processSerialCommands()
void processSerialCommands(void) {
    static char buf[64];
    static uint8_t bufLen = 0;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (bufLen > 0) {
                buf[bufLen] = '\0';
                dispatchCommand(buf, bufLen);  // NEW: route to command handlers
                bufLen = 0;
            }
        } else if (bufLen < sizeof(buf) - 1) {
            buf[bufLen++] = c;
        } else {
            bufLen = 0;  // Buffer overflow: discard
        }
    }
}
```

### EEPROM Save/Load Implementation

```cpp
// Save all configuration to EEPROM
void saveConfig(void) {
    // Display menu state
    for (uint8_t i = 0; i < sizeof(dispArray1); i++) {
        EEPROM.update(dispArray1Address + i, dispArray1[i]);
    }
    EEPROM.update(dispArray2Address, dispArray2[0]);
    EEPROM.update(unitsAddress, units);
    EEPROM.update(clockOffsetAddress, clockOffset);
    EEPROM.put(odoAddress, odo);
    EEPROM.put(odoTripAddress, odoTrip);
    EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);
    // Add new parameters here as the config tool expands
    Serial.println(F("ok"));
}

// Load all configuration from EEPROM
void loadConfig(void) {
    for (uint8_t i = 0; i < sizeof(dispArray1); i++) {
        dispArray1[i] = EEPROM.read(dispArray1Address + i);
    }
    dispArray2[0] = EEPROM.read(dispArray2Address);
    units = EEPROM.read(unitsAddress);
    clockOffset = EEPROM.read(clockOffsetAddress);
    EEPROM.get(odoAddress, odo);
    EEPROM.get(odoTripAddress, odoTrip);
    EEPROM.get(fuelSensorRawAddress, fuelSensorRaw);
    Serial.println(F("ok"));
}
```

---

## 12. Python Application Specification

### GUI Framework

- **Primary:** `tkinter` (stdlib, zero-install)
- **Alternative:** `PyQt5` / `PySide6` for richer widgets (requires `pip install`)
- **Minimum Python version:** 3.8

### Library Requirements

```
pyserial>=3.5
Pillow>=9.0
tkinter (stdlib)
zlib (stdlib)
struct (stdlib)
```

### Tab Layout (11+ tabs)

#### Tab 1 — Connection

| Widget | Type | Description |
|--------|------|-------------|
| Port dropdown | Combobox | Lists available COM/tty ports |
| Baud rate | Combobox | Default 115200 |
| Connect button | Button | Opens serial port, sends `ping` |
| Status label | Label | "Connected" / "Disconnected" |
| Firmware version | Label | Populated after `version` command |

#### Tab 2 — Motor Configuration

One group box per motor (Motor 1, 2, 3, 4, S). Each contains:

| Widget | Type | Description |
|--------|------|-------------|
| Signal source | Combobox | MotorSource enum values |
| Sweep steps | Spinbox | 100–8000 |
| Input low | Spinbox | Physical input at needle minimum |
| Input mid | Spinbox | Physical input at needle midpoint |
| Input high | Spinbox | Physical input at needle maximum |
| Live preview | Canvas | Animated needle at current sensor value |

#### Tab 3 — Speed & Hall Sensor

| Widget | Description |
|--------|-------------|
| Speed source | Combobox (SpeedSource enum) |
| Revs per km | Spinbox |
| Teeth per rev | Spinbox |
| Hall speed min | Spinbox |
| Hall filter | Slider 1–255 |
| Speedometer max | Spinbox (mph × 100) |

#### Tab 4 — Engine RPM

| Widget | Description |
|--------|-------------|
| RPM source | Combobox |
| Cylinder count | Combobox (2, 3, 4, 5, 6, 8, 10, 12) |
| Debounce window (µs) | Spinbox |
| Engine RPM min | Spinbox |
| RPM filter | Slider 1–255 |

#### Tab 5 — Sensors & Curves

Sub-tabs for each sensor:
- **Thermistor** — curve editor (up to 8 points), voltage divider helper
- **Fuel Level** — curve editor, sender type presets (0–30Ω, 0–90Ω, 10–180Ω)
- **AV1 / AV2 / AV3** — raw filter sliders, optional curve editors

**Curve Editor Widget:**
- Table with N rows (mV | value)
- Add/Remove point buttons
- Live graph preview
- Preset loader dropdown

#### Tab 6 — CAN Bus

| Widget | Description |
|--------|-------------|
| Protocol | Combobox (CANProtocol enum) |
| Signal sources | Grid of comboboxes (one per signal: Oil Prs, Coolant, etc.) |
| OBD-II PIDs | Checkbox list for polled parameters |

#### Tab 7 — LED Tachometer

| Widget | Description |
|--------|-------------|
| LED count | Spinbox 1–64 |
| Shift RPM | Spinbox |
| Min display RPM | Spinbox |
| Warning LEDs | Spinbox |
| Shift LEDs | Spinbox |
| Live preview | Canvas showing LED strip simulation |

#### Tab 8 — Display Settings

| Widget | Description |
|--------|-------------|
| Display 1 screen order | Drag-and-drop list of 18 screen names |
| Display 2 active screen | Combobox |
| Screen enable checkboxes | One per screen (18 for D1, 10 for D2) |
| Unit system | Radio buttons: Metric / Imperial |

#### Tab 9 — Fault Warnings

| Widget | Description |
|--------|-------------|
| Oil pressure threshold | Spinbox (kPa) |
| Coolant temp threshold | Spinbox (°C or °F) |
| Battery voltage threshold | Spinbox (V) |
| Engine running RPM min | Spinbox |
| Low fuel threshold | Spinbox (%) |

#### Tab 10 — Clock & Fuel

| Widget | Description |
|--------|-------------|
| UTC offset | Spinbox −12 to +12 |
| Fuel tank capacity | Spinbox (gallons) |

#### Tab 11 — Splash Screens (v1.2)

| Widget | Description |
|--------|-------------|
| Slot 1 preview | 128×32 Canvas showing current uploaded image |
| Slot 1 load button | Opens file dialog (PNG/BMP/JPG) |
| Slot 1 upload button | Runs conversion + serial upload |
| Slot 1 test button | Sends `splash test 1` |
| Slot 1 clear button | Sends `splash clear 1` |
| (Repeat for Slot 2) | |

### Validation Rules

| Parameter | Rule |
|-----------|------|
| Sweep values | Must be ≥ 100 and ≤ 8000; warn if < 200 |
| Filter coefficients | Must be 1–255 |
| Curve X values | Must be strictly increasing (0 < x[0] < x[1] < … < 5000) |
| Curve Y values | No restriction, but warn if monotonically non-decreasing expected |
| Num LEDs | Must be ≤ 64 (MAX_LEDS) |
| WARN_LEDS + SHIFT_LEDS | Must be < NUM_LEDS/2 |
| Splash image | Must be exactly 512 bytes after conversion |

### File Format (.txt config files)

```
# Gauge V4 Configuration
# Generated by gauge-config-tool v1.0
# Date: 2025-01-01

m1_sweep=696
m2_sweep=696
ms_sweep=4032
filter_vbatt=32
# ... (one key=value per line; lines starting with # are comments)
```

---

## 13. Firmware Upload Procedure

### avrdude Integration

The PC tool can flash new firmware using `avrdude`:

```python
import subprocess, shutil

def upload_firmware(hex_path: str, port: str) -> tuple[bool, str]:
    avrdude = shutil.which("avrdude")
    if not avrdude:
        return False, "avrdude not found in PATH"
    cmd = [
        avrdude, "-c", "arduino", "-p", "m2560",
        "-P", port, "-b", "115200",
        "-U", f"flash:w:{hex_path}:i"
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    return result.returncode == 0, result.stderr
```

### Safety Checks Before Upload

1. Verify firmware `.hex` file has valid Intel HEX format (first byte `:`).
2. Warn if file size > 230 KB (near flash limit).
3. Prompt user to confirm: "This will overwrite existing firmware."
4. After upload, wait 3 seconds, then send `ping` to verify Arduino is responsive.
5. If `ping` fails, display "Upload may have failed — check connections."

### Upload Workflow

```
┌────────────────────────────────────────────────────┐
│  1. User selects .hex file                         │
│  2. Tool verifies file format                      │
│  3. User confirms port selection                   │
│  4. Tool closes active serial connection           │
│  5. avrdude flashes firmware                       │
│  6. Tool waits 3 seconds for bootloader exit       │
│  7. Tool re-opens serial port                      │
│  8. Tool sends "ping" → expects "pong"             │
│  9. Tool reads "version" → updates status bar      │
│ 10. Tool sends "load" → refreshes UI from EEPROM   │
└────────────────────────────────────────────────────┘
```

### Error Handling

| Error | Response |
|-------|----------|
| Port busy | "Close all other serial monitors, then retry." |
| avrdude timeout | "Check USB cable and board power." |
| CRC mismatch after upload | "Re-flash; file may be corrupted." |
| `ping` no response | "Upload may have failed. Try again." |

---

## 14. Implementation Checklists

### Arduino Side

- [ ] Implement `processSerialCommands()` dispatch table for `get`/`set`/`save`/`load`/`dump`
- [ ] Implement `serial_config.cpp`: parameter name→address lookup, type-safe read/write
- [ ] Add EEPROM addresses for all configuration parameters (extending globals.cpp)
- [ ] Implement `splash.h/cpp`: upload state machine, EEPROM write, CRC32, validity flag
- [ ] Add `dispCustomSplash()` call in startup sequence
- [ ] Add `version` command response (e.g., `version 4.0`)
- [ ] Add `ping` → `pong` command
- [ ] Add enable flags for all outputs (motor, LED, display, CAN TX)
- [ ] Add screen ordering permutation array to EEPROM
- [ ] Test all commands at 115200 baud; ensure no blocking during CAN/GPS processing

### Python Side

- [ ] Implement `serial_protocol.py`: `connect()`, `get()`, `set()`, `save()`, `load()`, `dump()`
- [ ] Implement `ssd1306_converter.py`: `SSD1306Converter.convert()`, `.preview()`
- [ ] Implement `splash_uploader.py`: `SplashUploader.upload()`, `.test()`, `.clear()`
- [ ] Implement `config_file.py`: `save_to_file()`, `load_from_file()` in key=value format
- [ ] Build GUI: all 11 tabs with correct widgets and validation
- [ ] Implement curve editor widget with live graph preview
- [ ] Implement LED strip live preview in tachometer tab
- [ ] Implement needle preview animation in motor config tab
- [ ] Implement avrdude firmware upload wrapper
- [ ] Add "Read from device" and "Write to device" toolbar buttons
- [ ] Add progress bar for splash upload

### Testing

- [ ] Verify `curveLookup()` handles out-of-range inputs (clamp, not crash)
- [ ] Verify splash CRC32 matches between Python and Arduino implementations
- [ ] Verify EEPROM values survive power cycle
- [ ] Verify motor enable flags prevent calculation and stepping
- [ ] Verify `units` toggle switches all displayed values (km/h↔mph, °C↔°F, kPa↔PSI)
- [ ] Verify serial command buffer handles overflow gracefully
- [ ] Verify avrdude integration works on Windows and Linux

---

## 15. Testing Plan

### Unit Tests (Python)

```python
# tests/test_ssd1306.py
from ssd1306_converter import SSD1306Converter
from PIL import Image

def test_output_size():
    conv = SSD1306Converter()
    img = Image.new("1", (128, 32))
    data = conv.convert_pil(img)
    assert len(data) == 512

def test_roundtrip():
    conv = SSD1306Converter()
    img = Image.new("1", (128, 32))
    img.putpixel((0, 0), 1)   # Set top-left pixel
    data = conv.convert_pil(img)
    recovered = conv.preview(data)
    assert recovered.getpixel((0, 0)) == 255  # White pixel in "L" mode

def test_crc32_matches():
    import zlib
    data = bytes(range(256)) * 2  # 512 bytes
    py_crc = zlib.crc32(data) & 0xFFFFFFFF
    # Must match Arduino crc32() for same input
    assert py_crc == 0xE5C67BDE  # Known-good value for bytes(range(256))*2
```

```python
# tests/test_curve.py
from curve_lookup import curve_lookup

def test_clamp_low():
    xs = [100, 500, 1000]
    ys = [0, 50, 100]
    assert curve_lookup(0, xs, ys) == 0

def test_clamp_high():
    xs = [100, 500, 1000]
    ys = [0, 50, 100]
    assert curve_lookup(2000, xs, ys) == 100

def test_interpolation():
    xs = [0, 1000]
    ys = [0, 100]
    assert curve_lookup(500, xs, ys) == 50
```

### Integration Tests

| Test | Method | Expected |
|------|--------|----------|
| Connect to Arduino | Send `ping` | Receive `pong` within 500ms |
| Read all parameters | Send `dump` | All params in expected format |
| Write and read back | `set m1_sweep 800` then `get m1_sweep` | Returns `m1_sweep 800` |
| EEPROM persistence | `save`, power-cycle, `get m1_sweep` | Returns `m1_sweep 800` |
| Splash upload | Upload 512-byte image, `splash test 1` | Display shows image |
| Splash CRC failure | Send corrupted chunk, wrong CRC | Returns `err: crc` |
| Range validation | `set num_leds 100` | Returns `err: range` |

### User Acceptance Criteria

1. A user with no C++ knowledge can change the speedometer maximum without editing firmware.
2. A user can upload a custom splash image from a PNG file in under 30 seconds.
3. After power cycling, all configuration changes persist.
4. The GUI clearly indicates which fields have unsaved changes.
5. Tooltip help is available for every parameter.

### Performance Benchmarks

| Operation | Target | Notes |
|-----------|--------|-------|
| `dump` response time | < 500ms | All parameters at 115200 baud |
| Splash upload time | < 15s | 512 bytes at 115200 baud with framing |
| GUI startup time | < 3s | On a 5-year-old laptop |
| Curve editor graph redraw | < 100ms | On point add/remove |

---

## Appendix A: Pin Assignment Reference

| Pin | Type | Signal | Notes |
|-----|------|--------|-------|
| A0 | Analog in | Battery voltage | Voltage divider R1=10k, R2=3.3k |
| A3 | Analog in | Fuel level sensor | Resistive float sender |
| A4 | Analog in | Thermistor | GM-style NTC |
| A5 (PIN_AV1) | Analog in | Barometric pressure | 30 PSI MAP sensor |
| A6 (PIN_AV2) | Analog in | Reserved AV2 | |
| A7 (PIN_AV3) | Analog in | Reserved AV3 | |
| 1 | Digital in | Rotary encoder button | INPUT_PULLUP |
| 2 | Digital in | Rotary encoder A | Interrupt 0 |
| 3 | Digital in | Rotary encoder B | Interrupt 1 |
| 5 | Digital out | OLED CS 1 (SPI) | Display 1 |
| 6 | Digital out | OLED DC 1 (SPI) | Display 1 |
| 7 | Digital out | OLED RST 1 (SPI) | Display 1 |
| 8–11 | Digital out | Odometer motor coils 1–4 | 28BYJ-48 wave drive |
| 18 | Digital in | MCP2515 CAN interrupt | |
| 20 | Digital in | Hall effect speed sensor | Interrupt; interrupt-capable |
| 21 | Digital in | Ignition coil tach input | Interrupt; via optocoupler |
| 22 | Digital out | WS2812 LED data | Tachometer strip |
| 26 | Digital out | OLED RST 2 (SPI) | Display 2 |
| 28 | Digital out | OLED DC 2 (SPI) | Display 2 |
| 29 | Digital out | OLED CS 2 (SPI) | Display 2 |
| 32 | Digital out | Motor 3 DIR | Speedometer |
| 33 | Digital out | Motor 3 STEP | Speedometer |
| 34 | Digital out | Motor 2 STEP | |
| 35 | Digital out | Motor 2 DIR | |
| 36 | Digital out | Motor RST (shared) | All SwitecX12 drivers |
| 37 | Digital out | Motor 1 STEP | |
| 38 | Digital out | Motor 1 DIR | |
| 40 | Digital out | Motor 4 STEP | |
| 41 | Digital out | Motor 4 DIR | |
| 45 | Digital out | Motor S STEP | Speedometer NEMA14/TMC2209 |
| 47 | Digital out | Motor S DIR | |
| 49 | Digital out | Power latch | Holds system on after ignition off |
| 53 | Digital out | MCP2515 CAN CS (SPI) | |

## Appendix B: CAN Protocol Reference

### Haltech V2 Message IDs

| CAN ID | Parameters | Scaling |
|--------|-----------|---------|
| 0x360 | RPM, MAP, TPS | RPM direct; MAP in kPa×10; TPS in %×10 |
| 0x361 | Fuel pressure, Oil pressure | kPa×10 (absolute) |
| 0x362 | Injector duty, Ignition angle | %×10; deg×10 |
| 0x368 | Lambda (AFR1) | AFR×1000 |
| 0x369 | Knock level | Raw |
| 0x3E0 | Coolant, Air, Fuel, Oil temp | K×10 (coolant/air/fuel); C×10 (oil) |
| 0x3E1 | Trans temp, Fuel composition | C×10; %×10 |
| 0x470–0x473 | Wheel speeds FL/FR/RL/RR | km/h×10 |

### Megasquirt Message IDs (Base 0x5F0)

| CAN ID | Parameters |
|--------|-----------|
| 0x5F0 | MAP (kPa×10 LE), RPM |
| 0x5F1 | Coolant temp (°F×10 LE → convert to K×10) |
| 0x5F2 | TPS |
| 0x5F3 | AFR1 (×10 → ×1000) |
| 0x5F4 | Knock |
| 0x5EC | Vehicle speed (km/h×10 LE) |

### AiM Message IDs

| CAN ID | Parameters |
|--------|-----------|
| 0x0B0 | RPM (BE), Speed km/h×10 (BE) |
| 0x0B1 | Coolant temp °C×10 (BE), Oil temp °C×10 (BE) |
| 0x0B2 | MAP mbar (BE), Oil prs bar×10 (BE), Fuel prs bar×10 (BE) |
| 0x0B3 | Lambda×1000 (BE) |

### OBDII PIDs (Polled, Mode 0x01)

| PID | Parameter | Scaling |
|-----|-----------|---------|
| 0x0C | Engine RPM | ×0.25 RPM |
| 0x0D | Vehicle speed | km/h direct |
| 0x05 | Coolant temp | °C + 40 |
| 0x0B | MAP | kPa direct |
| 0x24 | O2 Sensor 1 Lambda | bytes 2–3 / 32768 |

---

*End of Technical Specification v1.2*
