# CAN Protocol Selection - Quick Reference

## How to Change Protocol (3 Steps)

### Step 1: Edit config_calibration.cpp
Open `gauge_V4/config_calibration.cpp` and find this line (around line 72):

```cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;
```

### Step 2: Change to Your Protocol
Replace with one of these values:

```cpp
// For Haltech v2 (default):
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;

// For Megasquirt:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_MEGASQUIRT;

// For AiM:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_AIM;

// For OBDII:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_OBDII;
```

### Step 3: Upload Firmware
Upload the updated firmware to your Arduino.

---

## Protocol Comparison Quick Table

| Feature | Haltech v2 | Megasquirt | AiM | OBDII |
|---------|-----------|------------|-----|-------|
| **Broadcast Mode** | ✓ | ✓ | ✓ | ✗ (polls) |
| **Update Rate** | Fast | Fast | Fast | Slow |
| **Byte Order** | Big | Little | Big | Standard |
| **Baud Rate** | 500k | 500k | 1000k* | 500k |
| **All 8 Params** | ✓ | ✗ | ✓ | ✗ |

*May vary

---

## What Parameters Work?

### All Protocols Support:
- Engine RPM  
- Coolant Temperature
- Lambda (AFR)
- Manifold Pressure (MAP)

### AiM & OBDII Also Support:
- Vehicle Speed (set `SPEED_SOURCE=1` to use CAN speed)

### Haltech v2 & AiM Only:
- Fuel Pressure
- Oil Pressure
- Oil Temperature

### Notes:
- **Megasquirt:** No oil/fuel pressure by default
- **OBDII:** No oil temp/pressure in standard PIDs
- **Haltech/Megasquirt:** No vehicle speed - use GPS or Hall sensor
- **AiM/OBDII:** Vehicle speed available - set `SPEED_SOURCE=1` to use

---

## Troubleshooting Quick Fixes

### No Data?
1. Check CAN wiring (CAN-H pin 6, CAN-L pin 14 for OBDII)
2. Verify 120Ω termination resistors
3. Check baud rate matches ECU (usually 500kbps)
4. Enable serial debug to see raw messages

### Wrong Values?
1. Verify correct protocol selected
2. Check byte order (Big vs Little Endian)
3. Confirm ECU is using standard protocol settings

### OBDII Slow?
1. This is normal - uses polling not broadcast
2. Consider switching to native ECU protocol if available

---

## Enable Debug Mode

To see raw CAN messages:

1. Open `gauge_V4/can.cpp`
2. Find the `receiveCAN()` function (line ~62)
3. Uncomment lines 74-94 (the debug code)
4. Upload firmware
5. Open Serial Monitor at 115200 baud

You'll see:
```
Standard ID: 0x360  DLC: 8  Data: 0x12 0x34 0x56 0x78 ...
```

---

## Common ECU Settings

### Haltech
- Protocol: Haltech v2
- Baud: 500 kbps
- Mode: Broadcast enabled

### Megasquirt with TunerStudio
- Protocol: Megasquirt
- Baud: 500 kbps
- Base ID: 0x5F0 (default)
- Mode: Broadcast enabled

### AiM ECUs
- Protocol: AiM
- Baud: 1000 kbps (check manual)
- Mode: Broadcast enabled

### Generic OBDII Vehicle
- Protocol: OBDII
- Baud: 500 kbps
- Mode: Polling (automatic)

---

## Full Documentation

See these files for complete details:
- `documentation/CAN_PROTOCOL_SELECTION.md` - Complete guide
- `documentation/CAN_PROTOCOL_EXAMPLES.md` - Examples & troubleshooting

---

## Need Help?

1. Check your ECU's CAN protocol documentation
2. Verify message IDs match expected values
3. Use debug mode to see what messages are being received
4. Consult protocol reference PDFs linked in documentation

---

**Current Default:** Haltech v2 (CAN_PROTOCOL_HALTECH_V2)

**To Change:** Edit one line in `config_calibration.cpp`, upload, done!
