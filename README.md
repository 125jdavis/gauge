# Gauge Controller

An instrument panel controller for vintage vehicles with modern internals. Receives inputs from GPS, analog sensors, and CAN bus, then outputs to stepper motors, OLED displays, LED tachometer, and CAN bus.

## Overview

This project is designed to retrofit vintage instrument panels with modern digital internals while maintaining the original appearance. The system is modular and configurable to work with various sensor inputs and ECU protocols.

### Features

- **4x Stepper Motors** - Smooth gauge needle control (fuel, temp, speedometer, etc.)
- **2x OLED Displays** - Configurable multi-page display system (128x32 pixels)
- **LED Tachometer** - WS2812 LED strip with shift light
- **CAN Bus Interface** - Haltech, Megasquirt, AiM, OBDII protocols supported
- **GPS Integration** - Speed, odometer, and clock from GPS module
- **Analog Sensors** - Battery voltage, fuel level, thermistors, pressure sensors
- **Hall Sensor** - Alternative speed input with VR-safe filtering
- **Ignition Pulse** - Direct engine RPM measurement from coil
- **Rotary Encoder** - Menu navigation and settings adjustment
- **EEPROM Storage** - Saves settings and odometer between power cycles

## Hardware Platforms

### ✅ STM32F407 (Current - V5 Board)

**Target Board:** pazi88 MEGA F407 (STM32F407VE)

The current version (V5) uses the STM32F407 microcontroller with native CAN controller and enhanced performance.

**Key Specifications:**
- 168 MHz ARM Cortex-M4
- 512KB Flash, 192KB RAM
- 12-bit ADC (0-4095), 3.3V reference
- Native bxCAN controller
- Hardware timers for smooth motor control

**Documentation:**
- [STM32F407 Port Guide](documentation/STM32F407_PORT.md) - Complete porting guide
- [Pin Mapping Reference](documentation/PIN_MAPPING_STM32.md) - Quick pin reference

### 📦 Arduino Mega 2560 (Legacy - V4 Board)

The original version (V4) used Arduino Mega 2560 with MCP2515 CAN controller.

**Note:** V4 code is preserved in git history but the main branch now targets STM32F407.

## Getting Started

### Prerequisites

**Hardware:**
- pazi88 MEGA F407 board or compatible STM32F407 board
- ST-Link V2 programmer (for uploading)
- CAN transceiver (typically on board)
- Stepper motors (SwitecX12 or compatible)
- OLED displays (SSD1306, 128x32)
- WS2812 LED strip
- Adafruit GPS module
- Various sensors (see documentation)

**Software:**
- Arduino IDE 2.x or 1.8.x
- STM32duino board support
- Required libraries (see below)

### Installation

1. **Install Arduino IDE and STM32 Support:**
   ```
   - Install Arduino IDE from arduino.cc
   - Add STM32 board manager URL
   - Install "STM32 MCU based boards" via Board Manager
   ```

2. **Install Required Libraries:**
   ```
   - Adafruit_SSD1306
   - Adafruit_GFX
   - Adafruit_GPS
   - FastLED
   - STM32_CAN (https://github.com/pazi88/STM32_CAN)
   - SwitecX12
   - Rotary
   ```

3. **Configure Arduino IDE:**
   - Board: Generic STM32F4 series → Generic F407VETx
   - U(S)ART support: Enabled
   - Upload method: STM32CubeProgrammer (SWD)

4. **Compile and Upload:**
   - Open `gauge_V4/gauge_V4.ino`
   - Verify/Compile
   - Upload via STM32 Cube Programmer

**Detailed instructions:** See [STM32F407_PORT.md](documentation/STM32F407_PORT.md)

## Project Structure

```
gauge/
├── gauge_V4/              # Main source code
│   ├── gauge_V4.ino       # Main sketch file
│   ├── config_hardware.h  # Pin definitions
│   ├── config_calibration.h # Calibration parameters
│   ├── can.cpp/h          # CAN bus implementation
│   ├── sensors.cpp/h      # Sensor reading functions
│   ├── display.cpp/h      # OLED display functions
│   ├── gps.cpp/h          # GPS interface
│   ├── outputs.cpp/h      # Motor and LED control
│   ├── menu.cpp/h         # Menu system
│   └── hal_conf_extra.h   # STM32 CAN HAL config
│
└── documentation/         # Project documentation
    ├── STM32F407_PORT.md  # STM32 port guide
    ├── PIN_MAPPING_STM32.md # Pin reference
    ├── CAN_USER_GUIDE.md  # CAN protocol guide
    └── [other docs]       # Additional documentation
```

## Configuration

### Pin Mapping

All pin assignments are defined in `config_hardware.h`. The STM32F407 version uses different pins than the Arduino Mega. See [PIN_MAPPING_STM32.md](documentation/PIN_MAPPING_STM32.md) for complete mapping.

### Calibration

Sensor calibrations and scaling factors are in `config_calibration.cpp`. Adjust these values for your specific sensors and gauges.

### CAN Protocol

Select your ECU protocol in `config_calibration.h`:
- `CAN_PROTOCOL_HALTECH_V2` - Haltech ECU (default)
- `CAN_PROTOCOL_MEGASQUIRT` - Megasquirt
- `CAN_PROTOCOL_AIM` - AiM ECU
- `CAN_PROTOCOL_OBDII` - Generic OBDII

See [CAN_USER_GUIDE.md](documentation/CAN_USER_GUIDE.md) for protocol details.

## Technical Notes

### STM32F407-Specific Considerations

1. **Analog Inputs:** All analog inputs use voltage dividers (9.1k/4.7k) to protect 3.3V ADC from 5V signals

2. **12-bit ADC:** Resolution increased from 10-bit (Arduino) to 12-bit (STM32)

3. **Native CAN:** Uses STM32 bxCAN controller instead of external MCP2515

4. **Timer-Based Updates:** 10 kHz motor update interrupt via TIM2

5. **Serial GPS:** UART3 polling-based reading (no interrupt required)

### Performance

- Motor update ISR: <5% CPU overhead @ 10 kHz
- Main loop frequency: ~1 kHz typical
- Display update: 10 Hz (configurable)
- CAN bus: 500 kbps
- GPS update: 5 Hz

## Troubleshooting

See [STM32F407_PORT.md](documentation/STM32F407_PORT.md#troubleshooting) for common issues and solutions.

**Quick checks:**
- Verify all libraries are installed
- Check CAN bus termination (120Ω resistors)
- Confirm analog voltage dividers are present
- Enable U(S)ART support in Arduino IDE
- Ensure hal_conf_extra.h is in sketch folder

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

[Specify your license here]

## Credits

- Original Arduino Mega version: Jesse Davis
- STM32F407 port: [Contributors]
- STM32_CAN library: pazi88
- pazi88 MEGA F407 board: pazi88

## References

- [STM32F407 Datasheet](https://www.st.com/resource/en/datasheet/stm32f407ve.pdf)
- [pazi88 MEGA F407](https://github.com/pazi88/STM32_Mega)
- [STM32_CAN Library](https://github.com/pazi88/STM32_CAN)
- [STM32duino](https://github.com/stm32duino)

## Support

For questions or issues:
- Check documentation in `/documentation`
- Review troubleshooting section
- Open an issue on GitHub

---

**Status:** ✅ STM32F407 port complete and ready for testing
**Last Updated:** February 2024
