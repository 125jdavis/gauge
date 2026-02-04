# STM32F407 Pin Mapping Quick Reference

This document provides a quick reference for all pin mappings from Arduino Mega 2560 to STM32F407 (pazi88 MEGA F407).

## Pin Mapping Table

| Arduino Mega | Gauge V4 Function | STM32F407 Pin | Pin Type | Notes |
|-------------|-------------------|---------------|----------|-------|
| A0 | VBATT | PA0 | Analog Input | 12-bit ADC, voltage divider |
| A3 | FUEL_PIN | PA3 | Analog Input | 12-bit ADC, voltage divider |
| A4 | THERM | PA4 | Analog Input | 12-bit ADC, voltage divider |
| A5 | PIN_AV1 | PA5 | Analog Input | 12-bit ADC, voltage divider |
| A6 | PIN_AV2 | PA6 | Analog Input | 12-bit ADC, voltage divider |
| A7 | PIN_AV3 | PA7 | Analog Input | 12-bit ADC, voltage divider |
| D2 | ROTARY_ENC (DT) | PD7 | 3.3V GPIO Input | Interrupt capable |
| D3 | ROTARY_ENC (CLK) | PB6 | 3.3V GPIO Input | Interrupt capable |
| D4 | OLED_RST_2 | PD11 | 5V Buffered Out | Display 2 reset |
| D5 | OLED_DC_2 | PD10 | 5V Buffered Out | Display 2 D/C |
| D6 | OLED_CS_2 | PD9 | 5V Buffered Out | Display 2 chip select |
| D7 | OLED_RST_1 | PD8 | 5V Buffered Out | Display 1 reset |
| D8 | MOSI | PB15 | 5V Buffered | SPI MOSI |
| D9 | MISO | PB14 | 5V Buffered | SPI MISO |
| D10 | OLED_DC_1 | PB12 | 5V Buffered Out | Display 1 D/C |
| D11 | SCK | PB13 | 5V Buffered | SPI SCK |
| D12 | OLED_CS_1 | PA8 | 5V Buffered Out | Display 1 chip select |
| D14 | SWITCH | PD5 | 3.3V GPIO Input | Rotary encoder button |
| D16 | GPS_TX (board RX) | PB10 | 3.3V UART3 TX | GPS transmit |
| D17 | GPS_RX (board TX) | PB11 | 3.3V UART3 RX | GPS receive |
| D18 | PWR_PIN | PD4 | 3.3V GPIO Out | Power control |
| D19 | HALL_PIN | PD3 | 3.3V GPIO Input | Hall speed sensor |
| D26 | LS_OUTPUT | PC7 | 5V Buffered Out | Light switch output |
| D28 | ODO_PIN4 | PC6 | 5V Buffered Out | Odometer motor |
| D30 | ODO_PIN2 | PD14 | 5V Buffered Out | Odometer motor |
| D31 | ODO_PIN3 | PD15 | 5V Buffered Out | Odometer motor |
| D32 | M1_STEP | PD12 | 5V Buffered Out | Motor 1 step |
| D33 | ODO_PIN1 | PD13 | 5V Buffered Out | Odometer motor |
| D34 | MS_M2 | PE5 | 3.3V GPIO Out | Motor S microstep |
| D35 | M1_DIR | PE15 | 5V Buffered Out | Motor 1 direction |
| D36 | MS_M1 | PE4 | 3.3V GPIO Out | Motor S microstep |
| D37 | M4_STEP | PE14 | 5V Buffered Out | Motor 4 step |
| D38 | MS_DIR | PE3 | 3.3V GPIO Out | Motor S direction |
| D39 | M4_DIR | PE13 | 5V Buffered Out | Motor 4 direction |
| D40 | MS_STEP | PE2 | 3.3V GPIO Out | Motor S step |
| D41 | M2_DIR | PE12 | 5V Buffered Out | Motor 2 direction |
| D42 | MOTOR_RST | PE10 | 5V Buffered Out | Motor reset |
| D45 | M2_STEP | PE11 | 5V Buffered Out | Motor 2 step |
| D47 | M3_DIR | PE9 | 5V Buffered Out | Motor 3 direction |
| D49 | M3_STEP | PE8 | 5V Buffered Out | Motor 3 step |
| D50 | TACH_DATA_PIN | PE7 | 5V Buffered Out | WS2812 LED data |
| D53 | IGNITION_PULSE_PIN | PB9 | 3.3V GPIO Input | Engine RPM sensor |
| - | CAN_TX | PA12 | CAN1 Peripheral | Native CAN TX |
| - | CAN_RX | PA11 | CAN1 Peripheral | Native CAN RX |

## Analog Input Voltage Divider

**IMPORTANT:** All analog inputs use voltage dividers to protect the 3.3V STM32 ADC from 5V signals.

**Resistor Values:**
- R1 (top, to signal): 4.7 kΩ
- R2 (bottom, to ground): 9.1 kΩ

**Voltage Conversion:**
```
Divider Ratio = R1 / (R1 + R2) = 4.7 / 13.8 = 0.3406
Input Voltage = ADC_reading * (3.3V / 4095) / 0.3406
```

**Example:**
- 5V input → 1.703V at ADC (5V * 0.3406)
- ADC reading: 2115 (1.703V / 3.3V * 4095)
- Recovered voltage: 2115 * (3.3/4095) / 0.3406 = 5.0V

## SPI Configuration

| Signal | STM32 Pin | Function |
|--------|-----------|----------|
| SCK | PB13 | SPI clock |
| MISO | PB14 | Master in, slave out |
| MOSI | PB15 | Master out, slave in |
| CS1 | PA8 | Display 1 chip select |
| CS2 | PD9 | Display 2 chip select |

**Note:** 5V level-shifted outputs for compatibility with 5V SPI devices.

## UART Configuration

| UART | TX Pin | RX Pin | Function | Baud Rate |
|------|--------|--------|----------|-----------|
| Serial (USB) | - | - | Debug/Programming | 115200 |
| Serial3 | PB10 | PB11 | GPS Module | 9600 |

## CAN Bus

| Signal | STM32 Pin | Function |
|--------|-----------|----------|
| CAN_TX | PA12 | CAN1 transmit |
| CAN_RX | PA11 | CAN1 receive |

**Configuration:**
- Baud rate: 500 kbps
- Native bxCAN controller (not external MCP2515)
- Requires 120Ω termination resistors on bus

## Motor Control

### Gauge Motors (SwitecX12)

| Motor | Step Pin | Dir Pin | Function |
|-------|----------|---------|----------|
| M1 | PD12 | PE15 | Fuel level |
| M2 | PE11 | PE12 | Coolant temp |
| M3 | PE8 | PE9 | Speedometer |
| M4 | PE14 | PE13 | Fuel level |

### Speedometer Motor (SwitecX12 with microstep)

| Signal | Pin | Function |
|--------|-----|----------|
| Step | PE2 | Step pulse |
| Dir | PE3 | Direction |
| M1 | PE4 | Microstep 1 |
| M2 | PE5 | Microstep 2 |
| Reset | PE10 | Motor reset (shared) |

### Odometer Motor (20BYJ-48)

| Coil | Pin | Function |
|------|-----|----------|
| 1 | PD13 | Phase A |
| 2 | PD14 | Phase B |
| 3 | PD15 | Phase C |
| 4 | PC6 | Phase D |

## Display Pins

### Display 1 (SSD1306)

| Signal | Pin | Function |
|--------|-----|----------|
| CS | PA8 | Chip select |
| DC | PB12 | Data/Command |
| RST | PD8 | Reset |
| MOSI | PB15 | Data (shared SPI) |
| SCK | PB13 | Clock (shared SPI) |

### Display 2 (SSD1306)

| Signal | Pin | Function |
|--------|-----|----------|
| CS | PD9 | Chip select |
| DC | PD10 | Data/Command |
| RST | PD11 | Reset |
| MOSI | PB15 | Data (shared SPI) |
| SCK | PB13 | Clock (shared SPI) |

## Interrupt-Capable Pins

The following pins are configured to use external interrupts:

| Function | Pin | Trigger | ISR Function |
|----------|-----|---------|--------------|
| Hall Speed Sensor | PD3 | Falling | `hallSpeedISR()` |
| Ignition Pulse (RPM) | PB9 | Falling | `ignitionPulseISR()` |
| Rotary Encoder DT | PD7 | Change | `rotate()` |
| Rotary Encoder CLK | PB6 | Change | `rotate()` |

## Hardware Timer Usage

| Timer | Function | Frequency |
|-------|----------|-----------|
| TIM2 | Motor update ISR | 10 kHz |

**Note:** Unlike Arduino Mega, GPS does not use a hardware timer interrupt. Data is read via polling in the main loop.

## Code Examples

### Reading Analog Input

```cpp
// Arduino Mega (10-bit, 0-5V)
int raw = analogRead(A0);
float voltage = raw * (5.0 / 1023.0);

// STM32F407 (12-bit, 0-3.3V with divider)
int raw = analogRead(PA0);
float voltage = raw * (3.3 / 4095.0) / 0.3406;  // Compensate for divider
```

### CAN Message Send

```cpp
// Arduino Mega (MCP2515)
byte data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
CAN0.sendMsgBuf(0x100, 0, 8, data);

// STM32F407 (Native CAN)
CAN_message_t msg;
msg.id = 0x100;
msg.len = 8;
for (int i = 0; i < 8; i++) msg.buf[i] = i + 1;
Can.write(msg);
```

### GPS Reading

```cpp
// Arduino Mega (Interrupt-based via Timer0)
// GPS data read automatically in ISR

// STM32F407 (Polling-based)
char c = GPS.read();  // Read in main loop
if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());
}
```

## Important Notes

1. **Voltage Levels:**
   - Most GPIO pins are 5V tolerant
   - ADC pins are NOT 5V tolerant (max 3.3V)
   - All analog inputs MUST use voltage dividers

2. **Pin Naming:**
   - Use STM32 names (e.g., `PA0`, `PB10`) not Arduino names (e.g., `A0`, `D10`)
   - Arduino pin numbers will not work with STM32

3. **Analog Resolution:**
   - STM32 uses 12-bit ADC (0-4095) vs Arduino 10-bit (0-1023)
   - Update all scaling formulas accordingly

4. **Interrupts:**
   - Use `digitalPinToInterrupt()` for compatibility
   - Not all pins support external interrupts (check datasheet)

5. **Serial Ports:**
   - Serial3 is used for GPS (not Serial2 as on Mega)
   - Serial (USB) is for debug output

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024-02-04 | Initial pin mapping document |
