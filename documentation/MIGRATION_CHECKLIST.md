# STM32F407 Migration Checklist

Use this checklist when migrating from Arduino Mega to STM32F407 hardware.

## Pre-Migration Planning

- [ ] Review [STM32F407_PORT.md](STM32F407_PORT.md) documentation
- [ ] Review [PIN_MAPPING_STM32.md](PIN_MAPPING_STM32.md) pin reference
- [ ] Verify you have STM32F407 board (pazi88 MEGA F407 or compatible)
- [ ] Ensure you have ST-Link V2 programmer
- [ ] Have voltage divider resistors ready (4.7kΩ and 9.1kΩ)

## Hardware Preparation

### Voltage Dividers (CRITICAL)

- [ ] Install voltage dividers on ALL analog inputs:
  - [ ] VBATT (PA0): 4.7k/9.1k divider
  - [ ] FUEL (PA3): 4.7k/9.1k divider
  - [ ] THERM (PA4): 4.7k/9.1k divider
  - [ ] PIN_AV1 (PA5): 4.7k/9.1k divider
  - [ ] PIN_AV2 (PA6): 4.7k/9.1k divider
  - [ ] PIN_AV3 (PA7): 4.7k/9.1k divider

**WARNING:** Connecting 5V directly to STM32 ADC pins will damage the chip!

### CAN Bus

- [ ] Connect CAN_TX to PA12
- [ ] Connect CAN_RX to PA11
- [ ] Verify 120Ω termination resistors on CAN bus
- [ ] Remove MCP2515 module (no longer needed)
- [ ] Remove MCP2515 wiring (CS, INT, SPI connections)

### GPS Module

- [ ] Connect GPS TX to PB10 (board RX)
- [ ] Connect GPS RX to PB11 (board TX)
- [ ] Verify GPS power is 5V
- [ ] Verify GPS logic levels are 3.3V compatible

### Display Connections

- [ ] OLED Display 1:
  - [ ] CS to PA8
  - [ ] DC to PB12
  - [ ] RST to PD8
- [ ] OLED Display 2:
  - [ ] CS to PD9
  - [ ] DC to PD10
  - [ ] RST to PD11
- [ ] Shared SPI:
  - [ ] SCK to PB13
  - [ ] MOSI to PB15
  - [ ] MISO to PB14

### Motor Connections

- [ ] Motor 1: Step=PD12, Dir=PE15
- [ ] Motor 2: Step=PE11, Dir=PE12
- [ ] Motor 3: Step=PE8, Dir=PE9
- [ ] Motor 4: Step=PE14, Dir=PE13
- [ ] Motor S: Step=PE2, Dir=PE3, M1=PE4, M2=PE5
- [ ] Motor Reset: PE10
- [ ] Odometer: Pin1=PD13, Pin2=PD14, Pin3=PD15, Pin4=PC6

### Sensors

- [ ] Hall speed sensor to PD3
- [ ] Ignition pulse (RPM) to PB9
- [ ] Rotary encoder DT to PD7
- [ ] Rotary encoder CLK to PB6
- [ ] Rotary encoder button to PD5

### Other

- [ ] LED tachometer data to PE7
- [ ] Power control to PD4
- [ ] LS output to PC7

## Software Setup

### Arduino IDE

- [ ] Install Arduino IDE (1.8.x or 2.x)
- [ ] Add STM32 board manager URL:
  ```
  https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
  ```
- [ ] Install "STM32 MCU based boards" package
- [ ] Configure board settings:
  - [ ] Board: Generic STM32F4 series
  - [ ] Board part number: Generic F407VETx
  - [ ] U(S)ART support: Enabled (generic 'Serial')
  - [ ] USB support: CDC
  - [ ] Upload method: STM32CubeProgrammer (SWD)

### Libraries

Install via Library Manager or manually:

- [ ] Adafruit_SSD1306
- [ ] Adafruit_GFX
- [ ] Adafruit_GPS
- [ ] FastLED
- [ ] SwitecX12
- [ ] Rotary
- [ ] STM32_CAN (https://github.com/pazi88/STM32_CAN)

### Code Files

- [ ] Copy all files from `gauge_V4/` folder
- [ ] Verify `hal_conf_extra.h` is present in sketch folder
- [ ] Open `gauge_V4.ino` in Arduino IDE

## Compilation

- [ ] Click Verify/Compile
- [ ] Resolve any library errors:
  - [ ] If "STM32_CAN.h not found": Install STM32_CAN library
  - [ ] If "HAL_CAN_MODULE_ENABLED not defined": Check hal_conf_extra.h exists
  - [ ] If "'Serial3' was not declared": Enable U(S)ART in board settings
- [ ] Compilation successful (no errors)
- [ ] Review warnings (should be minimal)

## Upload

### Using ST-Link V2 (Recommended)

- [ ] Connect ST-Link to board:
  - [ ] SWDIO to board SWDIO
  - [ ] SWCLK to board SWCLK
  - [ ] GND to board GND
  - [ ] 3.3V to board 3.3V (optional)
- [ ] Open STM32 Cube Programmer
- [ ] Connect to target (SWD interface)
- [ ] Load .bin file from Arduino build directory
- [ ] Program and verify
- [ ] Disconnect ST-Link
- [ ] Power cycle board

### Using DFU (If Bootloader Present)

- [ ] Set BOOT0 jumper to 1
- [ ] Reset or power cycle board
- [ ] Upload via Arduino IDE
- [ ] Set BOOT0 back to 0
- [ ] Reset board

## Initial Testing

### Power On

- [ ] Board powers up
- [ ] LED indicators working (if present)
- [ ] Serial output appears on console
- [ ] No smoke or burning smell

### Serial Debug

- [ ] Open Serial Monitor (115200 baud)
- [ ] See startup messages
- [ ] Motor update timer initialized message
- [ ] CAN filters configured message

### Motor Test

- [ ] Motors perform startup sweep
- [ ] All 5 motors move smoothly
- [ ] No jerky or stuttering motion
- [ ] Motors return to zero

### Display Test

- [ ] Display 1 shows splash screen
- [ ] Display 2 shows splash screen
- [ ] Displays update with data

### Sensor Test

- [ ] Check analog readings in serial output
- [ ] Battery voltage reading reasonable (10-15V)
- [ ] Fuel sensor reading present
- [ ] Temperature sensor reading present
- [ ] Analog values update regularly

### GPS Test

- [ ] GPS data appears in serial output
- [ ] Speed reading updates
- [ ] Time reading present
- [ ] Fix status shown

### CAN Test

- [ ] Connect to CAN bus
- [ ] CAN messages received (check serial debug)
- [ ] Engine parameters update
- [ ] No CAN errors

## Calibration

### Analog Sensors

For each analog sensor, verify readings match expected values:

- [ ] Battery voltage: Compare to multimeter
- [ ] Fuel level: Check against known level
- [ ] Temperature: Compare to known temperature
- [ ] Pressure sensors: Check against gauge

If readings are incorrect:
- Double-check voltage divider values
- Verify ADC scaling in config_calibration.h
- Check sensor output voltage range

### Motor Calibration

- [ ] Verify motor sweep ranges match gauge faces
- [ ] Adjust M1_SWEEP, M2_SWEEP, etc. if needed
- [ ] Test full range of motion
- [ ] Verify zero position is correct

### Speed Calibration

- [ ] Test speed reading against GPS or speedometer
- [ ] Adjust TEETH_PER_REV if using Hall sensor
- [ ] Verify odometer accumulation

## Final Verification

- [ ] All sensors reading correctly
- [ ] All motors moving smoothly
- [ ] Displays showing correct data
- [ ] CAN messages received
- [ ] GPS functioning
- [ ] Rotary encoder working
- [ ] LED tachometer functioning
- [ ] Settings save to EEPROM
- [ ] System stable for 15+ minutes

## Troubleshooting

If something doesn't work, refer to:

1. [STM32F407_PORT.md - Troubleshooting](STM32F407_PORT.md#troubleshooting)
2. [PIN_MAPPING_STM32.md](PIN_MAPPING_STM32.md) - Verify pin connections
3. Serial debug output for error messages

### Common Issues

**No CAN messages:**
- Check termination resistors (120Ω)
- Verify PA11/PA12 connections
- Check CAN bus baud rate (500 kbps)

**Wrong analog readings:**
- ALWAYS double-check voltage dividers present
- Verify divider ratio (4.7k/9.1k)
- Check ADC pin connections

**Motors jerky:**
- Verify timer frequency (10 kHz)
- Check motor power supply
- Ensure MOTOR_RST is HIGH

**GPS not working:**
- Check Serial3 connections (PB10/PB11)
- Verify GPS has power
- Test GPS with simple Serial3 echo sketch

**Displays blank:**
- Check SPI connections
- Verify CS, DC, RST pin assignments
- Test with Adafruit_SSD1306 example sketch

## Rollback Plan

If you need to revert to Arduino Mega:

- [ ] Keep original Arduino Mega hardware
- [ ] Git checkout previous commit before STM32 port
- [ ] Restore MCP2515 CAN module
- [ ] Reconnect all original pins
- [ ] Compile and upload original code

## Documentation

Keep these documents handy:

- [ ] STM32F407_PORT.md - Complete porting guide
- [ ] PIN_MAPPING_STM32.md - Pin reference
- [ ] README.md - Project overview
- [ ] STM32F407 datasheet
- [ ] pazi88 MEGA F407 schematic

## Notes

Use this space to document your specific setup:

```
Board serial number: ________________
Date migrated: ________________
Tested by: ________________

Special notes:
- 
- 
- 
```

## Success!

Once all items are checked and system is stable:

- [ ] Document any custom changes made
- [ ] Save configuration in version control
- [ ] Enjoy your upgraded gauge system!

---

**Remember:** Test thoroughly before deploying in vehicle. The STM32 port changes many fundamental aspects of the system.

**Safety:** Always verify analog voltage dividers are present before powering on with sensors connected.
