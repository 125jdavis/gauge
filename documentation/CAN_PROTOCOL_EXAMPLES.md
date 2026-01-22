# CAN Protocol Configuration Examples

## Quick Start

To change the CAN protocol, edit `gauge_V4/config_calibration.cpp` and change the `CAN_PROTOCOL` variable.

## Example Configurations

### Example 1: Haltech v2 (Default)

```cpp
// In config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;
```

**Available Parameters:**
- ✓ Vehicle Speed (wheel speed averaging)
- ✓ Engine RPM
- ✓ Coolant Temperature
- ✓ Fuel Pressure
- ✓ Oil Pressure
- ✓ Oil Temperature
- ✓ Lambda (AFR)
- ✓ Manifold Pressure (MAP)

**Notes:**
- Default configuration
- All parameters available
- Big Endian byte order
- 500 kbps CAN bus
- Vehicle speed from averaged wheel speeds (0x470-0x473)

---

### Example 2: Megasquirt

```cpp
// In config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_MEGASQUIRT;
```

**Available Parameters:**
- ✓ Vehicle Speed (VSS1)
- ✓ Engine RPM
- ✓ Coolant Temperature
- ✗ Fuel Pressure (not available)
- ✗ Oil Pressure (not available)
- ✗ Oil Temperature (not available)
- ✓ Lambda (AFR)
- ✓ Manifold Pressure (MAP)

**Notes:**
- Little Endian byte order
- 500 kbps CAN bus
- Temperature converted from Fahrenheit to Kelvin
- Oil/Fuel pressure require custom channel configuration in Megasquirt
- Vehicle speed from VSS1 sensor (0x5EC)

---

### Example 3: AiM ECU

```cpp
// In config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_AIM;
```

**Available Parameters:**
- ✓ Vehicle Speed
- ✓ Engine RPM
- ✓ Coolant Temperature
- ✓ Fuel Pressure
- ✓ Oil Pressure
- ✓ Oil Temperature
- ✓ Lambda (AFR)
- ✓ Manifold Pressure (MAP)

**Notes:**
- Big Endian byte order
- May use 1000 kbps CAN bus (verify with ECU documentation)
- All parameters available
- Pressure values converted from bar/mbar to kPa

---

### Example 4: OBDII (Generic ECU)

```cpp
// In config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_OBDII;
```

**Available Parameters:**
- ✓ Vehicle Speed (polled at 10Hz)
- ✓ Engine RPM (polled at 10Hz)
- ✓ Coolant Temperature (polled at 1Hz)
- ✗ Fuel Pressure (not available)
- ✗ Oil Pressure (not available)
- ✗ Oil Temperature (not available)
- ✓ Lambda (AFR) (polled at 10Hz, if supported)
- ✓ Manifold Pressure (MAP) (polled at 10Hz)

**Notes:**
- Active polling protocol (slower than broadcast)
- Priority 1 parameters update at 10Hz
- Priority 2 parameters update at 1Hz
- Lambda (PID 0x24) may not be available on older vehicles
- 500 kbps CAN bus
- Uses standard OBDII PIDs

---

## Verifying Your Configuration

After changing the protocol:

1. **Upload the firmware** to your Arduino
2. **Monitor the serial output** at 115200 baud
3. **Check for CAN messages** by uncommenting debug code in `can.cpp`
4. **Verify parameter updates** on the displays

### Debug Mode

To enable CAN message debugging, edit `gauge_V4/can.cpp` and uncomment the debug code in the `receiveCAN()` function (around line 74-94).

Example output:
```
Standard ID: 0x360       DLC: 8  Data: 0x12 0x34 0x56 0x78 0x9A 0xBC 0xDE 0xF0
Standard ID: 0x361       DLC: 8  Data: 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88
```

---

## Troubleshooting by Protocol

### Haltech v2 Issues

**Problem:** No data received
- Check that ECU is set to broadcast mode
- Verify CAN bus wiring (CAN-H, CAN-L)
- Confirm 500 kbps baud rate
- Ensure 120Ω termination resistors are installed

**Problem:** Incorrect RPM values
- Verify Big Endian byte order in ECU settings
- Check message ID 0x360 is being broadcast

---

### Megasquirt Issues

**Problem:** Temperature readings incorrect
- Megasquirt sends temperature in Fahrenheit
- Code automatically converts to Kelvin
- Verify your Megasquirt is sending temperature in °F * 10

**Problem:** Oil/Fuel pressure not available
- Standard Megasquirt doesn't broadcast these
- Configure custom channels in TunerStudio if needed
- Consider using analog sensors instead

---

### AiM Issues

**Problem:** No data received
- AiM may use 1000 kbps CAN instead of 500 kbps
- Check your ECU's CAN configuration
- Verify message IDs match your ECU (may be customized)

**Problem:** Pressure values incorrect
- AiM uses bar/mbar for pressures
- Code converts to kPa automatically
- Check ECU is sending in expected units

---

### OBDII Issues

**Problem:** Slow updates
- This is normal - OBDII is slower than broadcast protocols
- Priority 1 params: 10Hz (100ms per parameter rotation)
- Priority 2 params: 1Hz
- Consider switching to native ECU protocol if available

**Problem:** Lambda not available
- PID 0x24 is not supported on all vehicles
- Older vehicles (pre-2010) often don't support it
- Use a wideband O2 sensor with analog output instead

**Problem:** No response from ECU
- Verify ECU supports OBDII over CAN (post-2008 vehicles)
- Check CAN bus is connected to OBDII pins 6 (CAN-H) and 14 (CAN-L)
- Some vehicles use different CAN buses for powertrain vs body

---

## Multiple ECU Scenario

If you have multiple ECUs broadcasting on the same CAN bus:

1. Ensure message IDs don't conflict
2. Consider using CAN filters (not currently implemented)
3. For OBDII, responses come from 0x7E8-0x7EF (multi-ECU support included)

---

## Custom Protocol Configuration

If your ECU uses a custom protocol or non-standard message IDs, you can:

1. Create a new protocol parser (see `CAN_PROTOCOL_SELECTION.md`)
2. Or modify an existing parser to match your message IDs
3. Refer to your ECU's CAN protocol documentation

Example:
```cpp
// In can.cpp, modify Haltech parser for custom IDs
void parseCANHaltechV2(unsigned long id)
{
  if (id == 0x400) {  // Custom RPM message ID
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];
  }
  // ... rest of parsing
}
```

---

## Testing Checklist

After changing protocol:

- [ ] Firmware uploads successfully
- [ ] Serial debug shows CAN messages being received
- [ ] RPM displays correctly on gauge/display
- [ ] Vehicle speed updates (if using CAN for speed)
- [ ] Coolant temperature matches actual temperature
- [ ] Lambda/AFR shows reasonable values (14.7 for gasoline at idle)
- [ ] MAP shows atmospheric pressure (~100 kPa) at idle
- [ ] Oil/Fuel pressure shows expected values (if available)

---

For more detailed information, see `documentation/CAN_PROTOCOL_SELECTION.md`
