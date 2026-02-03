# STM32F407 Port Documentation

## Overview

This document describes the architectural changes made to port the gauge controller project from Arduino Mega 2560 to the STM32F407-based MEGA F407 board (pazi88/STM32_mega).

## Target Hardware

- **MCU**: STM32F407VET6
- **Board**: pazi88/STM32_mega (Arduino Mega form factor with STM32)
- **Core**: STM32duino (Arduino core for STM32)
- **IDE**: Arduino IDE
- **Flash Tool**: STM32 Cube Programmer

## Key Hardware Differences

### Microcontroller Capabilities

| Feature | Arduino Mega 2560 | STM32F407 |
|---------|------------------|-----------|
| CPU Speed | 16 MHz | 168 MHz |
| Flash | 256 KB | 512 KB |
| RAM | 8 KB | 128 KB |
| ADC Resolution | 10-bit (0-1023) | 12-bit (0-4095) |
| ADC Voltage | 0-5V | 0-3.3V |
| CAN Controller | External (MCP2515) | Built-in (bxCAN) |
| Timers | 6 (8/16-bit) | 14 (16/32-bit) |

### Pin Voltage Levels

⚠️ **CRITICAL**: STM32F407 pins are 3.3V logic, NOT 5V tolerant (except specific pins).
- Ensure all external sensors and modules use 3.3V logic or proper level shifters
- Analog inputs must not exceed 3.3V

## Pin Mapping Changes

### Complete Pin Mapping Table

| Function | Arduino Mega Pin | STM32F407 Pin | Notes |
|----------|------------------|---------------|-------|
| **Analog Sensors** |
| VBATT | A0 | PA0 | Battery voltage sensor |
| FUEL_PIN | A3 | PA3 | Fuel level sensor |
| THERM | A4 | PA4 | Thermistor temperature |
| PIN_AV1 | A5 | PA5 | Barometric pressure |
| PIN_AV2 | A6 | PA6 | Reserved sensor |
| PIN_AV3 | A7 | PA7 | Reserved sensor |
| **Rotary Encoder** |
| ROTARY_DT | D2 | PD7 | Encoder data pin |
| ROTARY_CLK | D3 | PB6 | Encoder clock pin |
| SWITCH | D1 | PD5 | Encoder button |
| **SPI Bus** |
| SCK | D52 | PB13 | SPI clock |
| MISO | D50 | PB14 | SPI MISO |
| MOSI | D51 | PB15 | SPI MOSI |
| **OLED Display 1** |
| OLED_CS_1 | D5 | PA8 | Chip select |
| OLED_DC_1 | D6 | PB12 | Data/Command |
| OLED_RST_1 | D7 | PD8 | Reset |
| **OLED Display 2** |
| OLED_CS_2 | D29 | PD9 | Chip select |
| OLED_DC_2 | D28 | PD10 | Data/Command |
| OLED_RST_2 | D26 | PD11 | Reset |
| **Odometer Motor** |
| ODO_PIN1 | D8 | PD13 | Coil 1 |
| ODO_PIN2 | D9 | PD14 | Coil 2 |
| ODO_PIN3 | D10 | PD15 | Coil 3 |
| ODO_PIN4 | D11 | PC6 | Coil 4 |
| LS_OUTPUT | - | PC7 | Light/signal output |
| **GPS Module** |
| GPS_TX (board RX) | RX2 | PB10 | GPS transmit (board receive) |
| GPS_RX (board TX) | TX2 | PB11 | GPS receive (board transmit) |
| **LED Tachometer** |
| TACH_DATA_PIN | D22 | PE7 | WS2812 LED strip data |
| **Stepper Motors** |
| M1_STEP | D37 | PD12 | Motor 1 step |
| M1_DIR | D38 | PE15 | Motor 1 direction |
| M2_STEP | D34 | PE11 | Motor 2 step |
| M2_DIR | D35 | PE12 | Motor 2 direction |
| M3_STEP | D33 | PE8 | Motor 3 step |
| M3_DIR | D32 | PE9 | Motor 3 direction |
| M4_STEP | D40 | PE14 | Motor 4 step |
| M4_DIR | D41 | PE13 | Motor 4 direction |
| MS_STEP | D45 | PE2 | Speedometer step |
| MS_DIR | D47 | PE3 | Speedometer direction |
| MS_M1 | - | PE4 | Speedometer microstep M1 |
| MS_M2 | - | PE5 | Speedometer microstep M2 |
| MOTOR_RST | D36 | PE10 | Motor driver reset (all) |
| **Sensors** |
| HALL_PIN | D20 | PD3 | Hall effect speed sensor |
| IGNITION_PULSE_PIN | D21 | PD2 | Ignition coil pulse (RPM) |
| COIL_NEG | - | PB9 | Coil negative control |
| **Power** |
| PWR_PIN | D49 | PD4 | Power control |
| **CAN Bus** |
| CAN_TX | D53 (SPI CS) | PA12 | Native CAN TX |
| CAN_RX | D18 (INT) | PA11 | Native CAN RX |

## Software Architecture Changes

### 1. CAN Bus Implementation

**Before (Arduino Mega):**
- External MCP2515 CAN controller via SPI
- Interrupt-driven message reception
- Library: `mcp_can.h`

**After (STM32F407):**
- Built-in bxCAN peripheral
- Direct hardware access
- Library: `STM32_CAN.h` (STM32duino CAN)

**Changes Made:**
- Added `canInit()`, `canReceive()`, `canSend()` functions for STM32
- Conditional compilation using `#ifdef STM32_CORE_VERSION`
- CAN filter configuration adapted for STM32 (14 filter banks vs 6 filters)
- Removed dependency on MCP2515 interrupt pin checking

### 2. Hardware Timers

**Before (Arduino Mega - AVR):**
- Timer3 for motor updates using `ISR(TIMER3_COMPA_vect)`
- Timer0 for GPS reading using `SIGNAL(TIMER0_COMPA_vect)`
- Direct register manipulation (TCCR3A, TCCR3B, OCR3A, TIMSK3)

**After (STM32F407):**
- HardwareTimer objects (TIM2 for motors, TIM3 for GPS)
- Callback functions instead of ISR macros
- High-level API for timer configuration

**Changes Made:**
```cpp
// Motor update timer (10 kHz)
HardwareTimer *motorTimer = new HardwareTimer(TIM2);
motorTimer->setOverflow(MOTOR_UPDATE_FREQ_HZ, HERTZ_FORMAT);
motorTimer->attachInterrupt(motorUpdateCallback);

// GPS reading timer (1 kHz)
HardwareTimer *gpsTimer = new HardwareTimer(TIM3);
gpsTimer->setOverflow(1000, HERTZ_FORMAT);
gpsTimer->attachInterrupt(gpsTimerCallback);
```

### 3. Analog-to-Digital Conversion (ADC)

**Before (Arduino Mega):**
- 10-bit ADC: 0-1023 range
- Reference voltage: 5V
- Direct mapping: `map(raw, 0, 1023, 0, 500)` for 0-5V

**After (STM32F407):**
- 12-bit ADC: 0-4095 range
- Reference voltage: 3.3V (lower maximum!)
- Adjusted mapping: `map(raw, 0, 4095, 0, 500)` for scaling

**Critical Voltage Consideration:**
```cpp
#ifdef STM32_CORE_VERSION
    // STM32: 0-4095 for 0-3.3V
    unsigned long newVal = map(raw, 0, 4095, 0, 500);
#else
    // Arduino Mega: 0-1023 for 0-5V
    unsigned long newVal = map(raw, 0, 1023, 0, 500);
#endif
```

⚠️ Sensor voltage dividers may need adjustment if originally designed for 5V ADC reference!

### 4. GPIO Pin References

**Before:**
- Pin numbers (e.g., `2`, `3`, `20`, `21`)
- `digitalPinToInterrupt(2)` for interrupt attachment

**After:**
- Port/Pin notation (e.g., `PA0`, `PB6`, `PD7`)
- `digitalPinToInterrupt(ROTARY_DT)` using defined constants
- All pin definitions updated in `config_hardware.h`

### 5. Serial Ports

**GPS Communication:**
- Arduino Mega: `Serial2` (hardware UART)
- STM32F407: `Serial2` (USART2 on PB10/PB11)
- ✅ No code changes needed - same Serial2 object

**Debug Output:**
- Both platforms: `Serial` (USB or UART1)

## Library Compatibility

### Required Libraries for STM32

| Library | Arduino Mega | STM32 Version | Status |
|---------|--------------|---------------|--------|
| Adafruit_SSD1306 | ✅ Compatible | ✅ Compatible | No changes needed |
| Adafruit_GFX | ✅ Compatible | ✅ Compatible | No changes needed |
| SPI | ✅ Built-in | ✅ Built-in | No changes needed |
| ~~mcp_can~~ | MCP2515 | N/A | ❌ Replaced |
| STM32_CAN | N/A | ✅ New | ✅ Added |
| Rotary | ✅ Compatible | ✅ Compatible | No changes needed |
| EEPROM | ✅ Built-in | ✅ Emulated | ⚠️ Different implementation |
| FastLED | ✅ Compatible | ✅ Compatible | No changes needed |
| Adafruit_GPS | ✅ Compatible | ✅ Compatible | Timer interrupt modified |
| SwitecX25 | ✅ Compatible | ✅ Compatible | No changes needed |
| SwitecX12 | ✅ Compatible | ✅ Compatible | No changes needed |

### EEPROM Considerations

STM32 has no built-in EEPROM. The Arduino core emulates EEPROM using Flash memory.

**Implications:**
- Limited write cycles (~10,000 vs 100,000 for AVR EEPROM)
- Wear leveling is NOT implemented
- Minimize EEPROM writes to extend Flash life
- Consider alternative storage (SD card, external EEPROM chip) for frequently-written data

**Current EEPROM Usage:**
- Odometer values (written occasionally)
- Display configuration (written on menu changes)
- Clock offset (written occasionally)
- Fuel sensor calibration (written occasionally)

✅ Current usage is acceptable but monitor wear over time.

## Conditional Compilation

Code uses preprocessor directives for platform-specific implementations:

```cpp
#ifdef STM32_CORE_VERSION
    // STM32-specific code
#else
    // Arduino Mega (AVR) code
#endif
```

This allows the codebase to remain compatible with both platforms.

## Build Configuration

### Arduino IDE Settings

**Board Selection:**
- Tools → Board → STM32 Boards → Generic STM32F4 series
- Tools → Board part number → STM32F407VE(T6)
- Tools → Upload method → STM32CubeProgrammer (SWD)
- Tools → USB support → CDC (generic 'Serial' supersede U(S)ART)
- Tools → U(S)ART support → Enabled (generic 'Serial')

**Libraries to Install:**
1. STM32duino (Boards Manager)
2. STM32duino CAN (Library Manager)
3. Adafruit SSD1306
4. Adafruit GFX
5. FastLED
6. Adafruit GPS
7. Rotary Encoder library

### Compilation Notes

- The code compiles for both Arduino Mega and STM32F407
- Use `#ifdef STM32_CORE_VERSION` for all platform-specific code
- Test thoroughly on real hardware (simulation cannot verify all peripherals)

## Flashing Procedure

1. **Compile** in Arduino IDE
2. **Export Compiled Binary** (Sketch → Export compiled Binary)
3. **Flash with STM32 Cube Programmer**:
   - Connect ST-Link or USB (DFU mode)
   - Load .bin file
   - Set address: 0x08000000
   - Click "Start Programming"

## Testing Checklist

After flashing, verify each subsystem:

- [ ] Serial debug output (115200 baud)
- [ ] GPS module communication
- [ ] Both OLED displays initialize and show content
- [ ] Rotary encoder responds to rotation and button press
- [ ] All 5 stepper motors perform sweep test
- [ ] Odometer motor responds
- [ ] LED tachometer strip lights up
- [ ] Analog sensors read values (battery voltage, fuel, temperature, pressure)
- [ ] Hall effect speed sensor triggers interrupt
- [ ] Ignition pulse (RPM) sensor triggers interrupt
- [ ] CAN bus initialization
- [ ] CAN message reception (if ECU connected)
- [ ] CAN message transmission
- [ ] EEPROM read/write (odometer, settings)
- [ ] Power control pin holds system on

## Known Issues and Limitations

### 1. CAN Filter Configuration
- STM32 CAN filters currently set to accept all messages
- Hardware filtering not yet optimized for specific protocols
- Software filtering still applied (no functional impact, slight CPU overhead)

### 2. ADC Voltage Range
- STM32 ADC maximum: 3.3V (vs 5V on Arduino Mega)
- **30 PSI pressure sensor limitation**: Original sensor outputs 0.5-4.5V (full 30 PSI range), but STM32 can only read up to 3.3V. Current implementation reads 0.5-3.0V, providing approximately 20 PSI max instead of 30 PSI.
- **Solutions**:
  - Use a voltage divider to scale 0-4.5V to 0-3.3V (preserves full range)
  - Replace sensor with 0-3.3V output version
  - Accept reduced pressure measurement range (0-20 PSI)
- Test all analog sensors to ensure proper scaling

### 3. Interrupt Pin Conflict Resolution
- **IGNITION_PULSE_PIN moved from PE7 to PD2** to avoid conflict with TACH_DATA_PIN
- LED tachometer (WS2812) uses PE7
- Verify pin assignments match your actual hardware wiring

### 4. EEPROM Wear
- Flash-based EEPROM has limited write cycles
- Monitor odometer updates for long-term reliability
- Consider external EEPROM for high-write-frequency data

### 5. Library Dependencies
- Ensure STM32duino CAN library is installed
- Some 3rd-party libraries may have AVR-specific code
- Test all features thoroughly

## Performance Improvements

The STM32F407 offers significant performance benefits:

| Metric | Arduino Mega 2560 | STM32F407 | Improvement |
|--------|------------------|-----------|-------------|
| CPU Speed | 16 MHz | 168 MHz | 10.5x |
| Flash | 256 KB | 512 KB | 2x |
| RAM | 8 KB | 128 KB | 16x |
| ADC Resolution | 10-bit | 12-bit | 4x |
| Timer Precision | Good | Excellent | - |
| CAN Bus | External SPI | Native | Lower latency |

**Practical Benefits:**
- Faster display updates
- More responsive menu navigation
- Better motor control precision
- Reduced CAN bus latency
- Room for future feature expansion

## Future Enhancements

With the extra capabilities of STM32F407, consider:

1. **Higher resolution displays** (more complex graphics)
2. **Data logging** to SD card (SPI or SDIO)
3. **Bluetooth/WiFi** connectivity (via external module on available UARTs)
4. **More complex signal processing** (GPS filtering, sensor fusion)
5. **Additional CAN buses** (STM32F407 has CAN1 and CAN2)
6. **Real-time data streaming** (via USB or external ethernet)

## Troubleshooting

### Compilation Errors

**"STM32_CAN.h: No such file or directory"**
- Install STM32duino CAN library via Arduino Library Manager

**"'TIM2' was not declared"**
- Ensure STM32 board is selected in Arduino IDE
- Re-check board configuration settings

### Runtime Issues

**No serial output:**
- Check USB cable and USB support setting (CDC enabled)
- Verify baud rate (115200)

**CAN not working:**
- Verify CAN transceiver connections (PA11=RX, PA12=TX)
- Check CAN bus termination (120Ω resistors)
- Confirm CAN bus voltage levels (recessive = 2.5V, dominant = 3.5V/1.5V)

**ADC readings incorrect:**
- Check sensor voltage (must not exceed 3.3V!)
- Verify voltage divider calculations
- Test with known voltage source

**Motors not moving:**
- Check motor driver power and reset pin
- Verify step/dir pin connections
- Ensure motor timer is running (check serial debug)

## References

- [STM32F407 Datasheet](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)
- [STM32F407 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00031020.pdf)
- [STM32duino Wiki](https://github.com/stm32duino/wiki/wiki)
- [pazi88/STM32_mega GitHub](https://github.com/pazi88/STM32_mega)
- [Arduino CAN Library for STM32](https://github.com/nopnop2002/Arduino-STM32-CAN)

## Version History

- **2026-02-03**: Initial STM32F407 port
  - Pin mappings updated
  - CAN bus migrated to native controller
  - Timers migrated to HardwareTimer
  - ADC scaling updated for 12-bit
  - All platform-specific code wrapped in conditionals

---

**Author**: Jesse Davis  
**Date**: February 3, 2026  
**Platform**: STM32F407VET6 (pazi88/STM32_mega)
