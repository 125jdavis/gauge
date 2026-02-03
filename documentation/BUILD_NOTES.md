# Build & Flash Instructions for STM32F407

## Prerequisites

### 1. Install Arduino IDE
Download and install Arduino IDE 2.0 or later from https://www.arduino.cc/en/software

### 2. Install STM32duino Core

1. Open Arduino IDE
2. Go to **File → Preferences**
3. Add this URL to "Additional Boards Manager URLs":
   ```
   https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "STM32" by STMicroelectronics
6. Install **STM32 MCU based boards** by STMicroelectronics

### 3. Install Required Libraries

Go to **Tools → Manage Libraries** and install:

| Library Name | Author | Version |
|--------------|--------|---------|
| Adafruit SSD1306 | Adafruit | Latest |
| Adafruit GFX Library | Adafruit | Latest |
| Adafruit GPS Library | Adafruit | Latest |
| FastLED | FastLED | Latest |
| Rotary | Ben Buxton | Latest |
| STM32duino STM32_CAN | pazi88 | Latest |

**Note**: SwitecX12 and SwitecX25 libraries may need manual installation if not in Library Manager.

## Board Configuration

### Arduino IDE Settings

1. **Tools → Board → STM32 Boards groups → Generic STM32F4 series**
2. **Tools → Board part number → STM32F407VE (or STM32F407VET6)**
3. **Tools → Upload method → STM32CubeProgrammer (SWD)** or **STM32CubeProgrammer (DFU)**
4. **Tools → USB support → CDC (generic 'Serial' supersede U(S)ART)**
5. **Tools → U(S)ART support → Enabled (generic 'Serial')**
6. **Tools → Optimize → Smallest (-Os default)**
7. **Tools → C Runtime Library → Newlib Nano (default)**

## Compilation

1. Open `gauge_V4/gauge_V4.ino` in Arduino IDE
2. Select the board and configure settings as above
3. Click **Verify/Compile** button (checkmark icon)
4. Wait for compilation to complete
5. Check for any errors in the output window

### Expected Compilation Output

```
Sketch uses XXXXX bytes (XX%) of program storage space. Maximum is 524288 bytes.
Global variables use XXXX bytes (X%) of dynamic memory, leaving XXXXX bytes for local variables. Maximum is 131072 bytes.
```

### Common Compilation Errors

**Error: "STM32_CAN.h: No such file or directory"**
- Solution: Install STM32duino CAN library (see Library Installation above)

**Error: "'TIM2' was not declared in this scope"**
- Solution: Verify board is set to STM32F407VE (not Arduino Mega)

**Error: "HardwareTimer was not declared in this scope"**
- Solution: Update STM32duino core to latest version

## Flashing to Hardware

### Method 1: STM32CubeProgrammer (Recommended)

1. **Export Binary from Arduino IDE**:
   - Sketch → Export compiled Binary
   - Binary file created in sketch folder: `gauge_V4.ino.bin`

2. **Install STM32CubeProgrammer**:
   - Download from: https://www.st.com/en/development-tools/stm32cubeprog.html
   - Install and launch

3. **Connect Hardware**:
   - **Option A - ST-Link V2**:
     - Connect SWDIO, SWCLK, GND, 3.3V to STM32F407 board
     - Select "ST-LINK" connection in STM32CubeProgrammer
   - **Option B - USB DFU**:
     - Set BOOT0 jumper to 1 (DFU mode)
     - Connect USB cable
     - Select "USB" connection in STM32CubeProgrammer
     - Reset board

4. **Flash Binary**:
   - Click "Connect" in STM32CubeProgrammer
   - Go to "Erasing & Programming" tab
   - File path: Browse to `gauge_V4.ino.bin`
   - Start address: `0x08000000`
   - Click "Start Programming"
   - Wait for "File download complete"
   - If using DFU mode: Set BOOT0 back to 0, reset board

### Method 2: Arduino IDE Direct Upload

**Requirements**: USB cable and DFU mode, OR ST-Link connected

1. Connect hardware (USB DFU or ST-Link as described above)
2. Select **Tools → Upload method → STM32CubeProgrammer (SWD)** or **(DFU)**
3. If using DFU: Set BOOT0 jumper to 1, reset board
4. Click **Upload** button (right arrow icon)
5. Wait for upload to complete
6. If using DFU: Set BOOT0 back to 0, reset board

## Verification After Flashing

### Serial Monitor Test

1. Connect USB cable to STM32F407 board
2. Open **Tools → Serial Monitor** in Arduino IDE
3. Set baud rate to **115200**
4. Press reset button on board
5. You should see startup messages:
   ```
   Motor update timer initialized (STM32): 10000 Hz
   STM32 CAN Initialized Successfully!
   CAN filters configured for protocol: X
   clockOffset: X
   ```

### Hardware Checklist

Use the checklist in `documentation/STM32_PORT.md` to verify all subsystems:
- [ ] Serial debug output working
- [ ] GPS communication
- [ ] OLED displays showing content
- [ ] Rotary encoder responding
- [ ] Motors performing sweep
- [ ] LED tachometer lighting
- [ ] Analog sensors reading
- [ ] CAN bus initializing

## Troubleshooting

### No Serial Output

- **Check**: USB cable and connection
- **Check**: "USB support" set to CDC in board configuration
- **Check**: Correct COM port selected in Tools → Port
- **Try**: Press reset button on board
- **Try**: Different USB cable or port

### "Error: target not found"

- **ST-Link**: Check wiring (SWDIO, SWCLK, GND, 3.3V)
- **ST-Link**: Try different ST-Link adapter
- **DFU**: Verify BOOT0 jumper in position 1
- **DFU**: Try different USB port or cable

### CAN Bus Not Working

- **Check**: CAN transceiver chip (usually TJA1050 or MCP2551)
- **Check**: CAN_TX (PA12) and CAN_RX (PA11) connections
- **Check**: 120Ω termination resistors on CAN bus
- **Check**: CAN bus power supply
- **Verify**: CAN bus idle voltage ~2.5V

### Motors Not Moving

- **Check**: MOTOR_RST pin (PE10) connected and HIGH
- **Check**: Motor driver power supply
- **Check**: Step/Dir pins connected correctly
- **Test**: Run motorSweepSynchronous() from setup()

### Analog Readings Wrong

- **CRITICAL**: Verify all analog inputs ≤ 3.3V (not 5V!)
- **Check**: Voltage dividers if sensors output > 3.3V
- **Test**: Use multimeter to measure actual pin voltages
- **Check**: ADC reference is 3.3V (internal on STM32)

### Display Not Working

- **Check**: SPI connections (SCK=PB13, MISO=PB14, MOSI=PB15)
- **Check**: Display CS, DC, RST pins
- **Check**: Display power (3.3V or 5V depending on module)
- **Try**: Swap displays to isolate hardware issue

## Development Tips

### Adding Debug Output

Use conditional compilation for debug messages:
```cpp
#ifdef DEBUG_MODE
  Serial.print("Debug: ");
  Serial.println(value);
#endif
```

### Testing Individual Components

Comment out sections in `setup()` to isolate issues:
```cpp
// ===== DISPLAY INITIALIZATION =====
// display1.begin(SSD1306_SWITCHCAPVCC);  // Temporarily disable
// display2.begin(SSD1306_SWITCHCAPVCC);
```

### Monitoring Performance

Check free RAM and CPU usage:
```cpp
extern "C" char* sbrk(int incr);
int freeRam() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}
```

### Using ST-Link Utilities

For advanced debugging:
- Use STM32CubeIDE for breakpoints and variable watching
- Use ST-Link Utility for memory inspection
- Enable SWV (Serial Wire Viewer) for real-time tracing

## Additional Resources

- **STM32F407 Datasheet**: https://www.st.com/resource/en/datasheet/stm32f407vg.pdf
- **STM32F407 Reference Manual**: https://www.st.com/resource/en/reference_manual/dm00031020.pdf
- **STM32duino GitHub**: https://github.com/stm32duino
- **STM32duino Forum**: https://www.stm32duino.com/
- **pazi88/STM32_mega**: https://github.com/pazi88/STM32_mega

## Support

For project-specific questions:
- Check `documentation/STM32_PORT.md` for detailed platform information
- Open an issue on the project GitHub repository

For STM32duino questions:
- Visit https://www.stm32duino.com/

---

**Last Updated**: February 3, 2026  
**Target Platform**: STM32F407VET6 (pazi88/STM32_mega board)
