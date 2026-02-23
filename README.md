# gauge

Arduino-based instrument panel controller for retrofitting vintage vehicles with modern instrumentation. Receives data from GPS, analog sensors, and CAN bus; drives stepper motor gauges, two OLED displays, and an LED tachometer strip.

---

## Features

**Outputs**
- 5× SwitecX12 stepper motors — speedometer, fuel level, coolant temp, and two auxiliary gauges
- Mechanical odometer motor (non-blocking, interrupt-driven)
- 2× 128×32 SSD1306 OLED displays (SPI), each independently configurable
- WS2812 LED tachometer strip with configurable warning and shift-light zones

**Display 1 screens** (scroll with rotary encoder): oil pressure, coolant temp, fuel level, battery voltage, RPM, speed, AFR, fuel pressure, boost (bar graph), boost (digital readout), oil temp, ethanol %, injector duty cycle, ignition timing, trip odometer, clock, logo, and a settings menu.

**Display 2** shows one persistent screen chosen from: oil pressure, coolant temp, fuel level, battery voltage, RPM, speed, boost (bar or text), clock, or logo.

**Inputs**
- CAN bus (MCP2515, 500 kbps) — supports Haltech v2, Megasquirt, AiM, and OBDII polling
- Adafruit GPS module — speed, time (5 Hz, NMEA RMC)
- Hall-effect wheel speed sensor
- Ignition coil pulse for engine RPM
- Analog sensors: fuel level, coolant/oil temperature (thermistor), battery voltage, barometric/manifold pressure (0–5 V)

**Signal source selection** — each parameter (speed, RPM, oil pressure, coolant temp, MAP, AFR, fuel level) can be independently routed from CAN, a dedicated analog sensor, or GPS. A synthetic generator is available for bench testing.

**Settings & persistence**
- Metric/imperial unit toggle
- Adjustable UTC clock offset (saved to EEPROM)
- Odometer and trip odometer (saved to EEPROM on shutdown)
- All display selections and calibration values persisted across power cycles

---

## Limitations

- **Hardware-specific** — requires Arduino Mega 2560, SwitecX12 stepper motors, and SSD1306 SPI displays. Porting to other hardware requires pin and library changes.
- **Mechanical odometer** — partially implemented; stepping logic is present but the feature is not fully functional.
- **Single CAN protocol** — only one ECU protocol can be active at a time; switching requires a firmware reflash.
- **OBDII polling** — update rate is lower (~1–10 Hz per parameter) compared to broadcast protocols (10–100 Hz).
- **No runtime calibration UI** — calibration parameters (sweep ranges, sensor curves, LED counts, etc.) are set in `config_calibration.cpp` and require recompilation.

---

## Hardware

| Component | Part |
|-----------|------|
| Microcontroller | Arduino Mega 2560 |
| CAN controller | MCP2515 (SPI, pin 53 CS) |
| OLED displays | 2× SSD1306 128×32 (SPI) |
| GPS | Adafruit GPS module (9600 baud) |
| Gauge motors | SwitecX12 stepper motors |
| Tachometer | WS2812 LED strip (pin 22) |
| Menu input | Rotary encoder with push button |

See `gauge_V4/config_hardware.h` for all pin assignments and `documentation/` for schematics and guides.
