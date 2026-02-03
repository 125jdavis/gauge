# gauge

This project is an STM32-based instrument panel controller. It receives inputs from GPS, analog, and CAN bus and outputs to 4x stepper motors for gauge pointers, LED warning lights, 2x OLED displays, and an LED tachometer. It is also able to send data via CAN to other microcontrollers on the bus.

The system is designed to be modular, intended to simplify retrofitting vintage instrument panels with modern internals.

## Platform Support

This codebase is designed for **STM32F407 (pazi88/STM32_mega board)**:
- Native CAN controller using HardwareCAN library (built-in)
- 12-bit ADC (0-3.3V with hardware voltage dividers for 5V sensors)
- 168 MHz ARM Cortex-M4 processor
- STM32duino 2.12.0 compatible

**Note:** Arduino Mega 2560 support has been removed. For the original Arduino Mega version, see earlier commits.

For details on the STM32F407 implementation, see:
- [documentation/STM32_PORT.md](documentation/STM32_PORT.md) - Hardware details and pin mappings
- [documentation/HARDWARECAN_MIGRATION.md](documentation/HARDWARECAN_MIGRATION.md) - CAN library migration guide
- [documentation/BUILD_NOTES.md](documentation/BUILD_NOTES.md) - Build and flash instructions
