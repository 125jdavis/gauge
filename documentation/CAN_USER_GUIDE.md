# CAN Protocol Support - User Guide

## Quick Start

Your gauge system supports 4 different CAN protocols. Choose the one that matches your ECU and you're ready to go!

### Step 1: Select Your Protocol

Edit `gauge_V4/config_calibration.cpp` and find this line (around line 71):

```cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;
```

Change it to match your ECU:

```cpp
// For Haltech ECU:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;

// For Megasquirt ECU:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_MEGASQUIRT;

// For AiM ECU:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_AIM;

// For OBDII (standard vehicles):
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_OBDII;
```

### Step 2: Upload Firmware

Upload the updated firmware to your Arduino Mega.

### Step 3: Check Serial Output

Open Serial Monitor at 115200 baud. You should see:
```
MCP2515 Initialized Successfully!
CAN filters configured for protocol: 0
```

**Done!** Your gauge will now read data from your ECU.

---

## What Data Can I Monitor?

Different ECUs provide different parameters. Here's what's available:

| Parameter | Haltech | Megasquirt | AiM | OBDII |
|-----------|---------|------------|-----|-------|
| **Vehicle Speed** | ✓ | ✓ | ✓ | ✓ |
| **Engine RPM** | ✓ | ✓ | ✓ | ✓ |
| **Coolant Temperature** | ✓ | ✓ | ✓ | ✓ |
| **Fuel Pressure** | ✓ | — | ✓ | — |
| **Oil Pressure** | ✓ | — | ✓ | — |
| **Oil Temperature** | ✓ | — | ✓ | — |
| **Lambda (AFR)** | ✓ | ✓ | ✓ | ✓ |
| **Manifold Pressure (MAP)** | ✓ | ✓ | ✓ | ✓ |

**✓** = Available  
**—** = Not available in this protocol

---

## Using CAN for Vehicle Speed

All protocols can provide vehicle speed via CAN. To use CAN speed:

**Edit `config_calibration.cpp`:**
```cpp
uint8_t SPEED_SOURCE = 1;  // 1 = CAN, 2 = Hall sensor, 3 = GPS
```

**Speed Sources by Protocol:**
- **Haltech:** Averages 4 wheel speeds (handles wheel slip)
- **Megasquirt:** VSS1 vehicle speed sensor
- **AiM:** Direct speed from ECU
- **OBDII:** Polled from ECU at 10 Hz

**Alternative:** Use GPS (`SPEED_SOURCE = 3`) or Hall sensor (`SPEED_SOURCE = 2`)

---

## Protocol Details

### Haltech v2

**Best For:** Haltech Elite/Platinum ECUs  
**CAN Speed:** 500 kbps  
**Update Rate:** Fast (ECU broadcasts continuously)

**What You Get:**
- All 8 parameters available
- Wheel speeds (FL, FR, RL, RR) averaged intelligently
- Handles wheel slip during acceleration/braking
- Full engine management data

**ECU Setup:**
- Enable CAN broadcast in NSP software
- Set CAN bus speed to 500 kbps
- Ensure standard message IDs are used

---

### Megasquirt

**Best For:** Megasquirt II, III with CAN  
**CAN Speed:** 500 kbps  
**Update Rate:** Fast (broadcasts continuously)

**What You Get:**
- Core parameters: RPM, temp, TPS, AFR, MAP
- Vehicle speed (VSS1 sensor)
- Knock detection

**Not Available:**
- Oil pressure/temperature (not in standard broadcast)
- Fuel pressure (not in standard broadcast)

**ECU Setup (TunerStudio):**
- Enable CAN broadcast
- Set base ID to 0x5F0 (default)
- Ensure VSS1 is configured for vehicle speed

**Note:** Oil/fuel pressure can be added as custom channels if your Megasquirt supports them.

---

### AiM

**Best For:** AiM data loggers and ECUs  
**CAN Speed:** 1000 kbps (may vary)  
**Update Rate:** Fast (broadcasts continuously)

**What You Get:**
- All 8 parameters available
- Very compact message format (only 4 CAN IDs)
- Full pressure and temperature data
- Direct vehicle speed

**ECU Setup:**
- Verify CAN bus speed (usually 1000 kbps)
- Check that standard AiM message IDs are used (0x0B0-0x0B3)
- If custom IDs configured, see troubleshooting section

**Note:** This is the most efficient protocol with minimal CAN traffic.

---

### OBDII

**Best For:** Stock/OEM ECUs, any vehicle with OBDII  
**CAN Speed:** 500 kbps  
**Update Rate:** Slower (polling-based)

**What You Get:**
- Basic parameters: RPM, speed, coolant temp, MAP
- Lambda/AFR (on vehicles with wideband O2, 2010+)
- Works with any OBDII-compliant vehicle

**Not Available:**
- Oil pressure/temperature (not standard OBDII PIDs)
- Fuel pressure (not standard OBDII PIDs)

**How It Works:**
- Gauge polls ECU for data (doesn't broadcast like others)
- Priority 1 parameters update 10x per second
- Priority 2 parameters update 1x per second
- Slower than other protocols but works universally

**Limitations:**
- Not all vehicles support all PIDs
- Lambda may not be available on older vehicles
- Update rate slower than broadcast protocols

**When to Use:** Stock ECU, no aftermarket standalone available

---

## Hardware Filtering (Performance Feature)

Your gauge uses advanced **hardware filtering** to reduce CPU load:

### What It Does

The MCP2515 CAN controller **rejects irrelevant messages at the hardware level** before they reach the Arduino. Only messages your ECU sends trigger interrupts.

**On a busy CAN bus (1000 messages/second):**
- Without filtering: 1000 Arduino interrupts/second
- With filtering: 10-20 Arduino interrupts/second
- **Result: 98% fewer interrupts, more CPU for your gauges!**

### How It Works

When you select a protocol, the system automatically configures hardware filters:

- **Haltech:** Only accepts 0x300-0x30F, 0x360-0x36F, 0x3E0-0x3EF, 0x470-0x47F
- **Megasquirt:** Only accepts 0x5E0-0x5EF, 0x5F0-0x5FF
- **AiM:** Only accepts 0x0B0-0x0BF
- **OBDII:** Only accepts 0x7E8-0x7EF

**All other CAN traffic is ignored.** No configuration needed - it's automatic!

---

## Troubleshooting

### No Data Showing Up

**Check 1: Verify Protocol Selection**
- Does `CAN_PROTOCOL` match your ECU type?
- Check serial output: "CAN filters configured for protocol: X"

**Check 2: CAN Bus Wiring**
```
Arduino Pin → MCP2515 → CAN Bus
D53 (CS)    → CS       
D19 (INT)   → INT      
D51 (MOSI)  → SI       
D50 (MISO)  → SO       
D52 (SCK)   → SCK      
            → CANH → ECU CAN-H
            → CANL → ECU CAN-L
```

**Check 3: CAN Bus Termination**
- Need 120Ω resistors at both ends of CAN bus
- Without termination, communication fails

**Check 4: CAN Bus Speed**
- Most protocols use 500 kbps ✓
- AiM may use 1000 kbps (check ECU settings)

### Some Parameters Not Updating

**Cause:** ECU may not broadcast all messages

**Solution 1: Check ECU Configuration**
- Haltech: Enable all broadcasts in NSP
- Megasquirt: Check which messages are enabled in TunerStudio
- AiM: Verify standard message IDs configured

**Solution 2: Verify Which Parameters Should Work**
- Refer to the "What Data Can I Monitor?" table above
- Some parameters genuinely not available in certain protocols

### Values Look Wrong

**Check 1: Correct Protocol Selected?**
- Wrong protocol = wrong data interpretation
- Example: Big Endian vs Little Endian will swap bytes

**Check 2: ECU Using Standard Message IDs?**
- Some ECUs allow customizing CAN message IDs
- Code expects standard protocol IDs
- Check ECU configuration

**Check 3: Units Correct?**
- Temperature: Should be in °C or °F on display
- Pressure: Should be in PSI or kPa
- Speed: Should match speedometer
- If off by factor of 10 or 100, may be conversion issue

### OBDII Specific Issues

**Slow Updates**
- This is normal for OBDII (polling-based)
- Priority 1: 10 updates/second
- Priority 2: 1 update/second
- Use native protocol if available (Haltech, Megasquirt, etc.)

**Lambda Not Available**
- Older vehicles (pre-2010) may not support PID 0x24
- Some vehicles never report lambda via OBDII
- Use wideband O2 sensor with analog output instead

**No Response from ECU**
- Verify vehicle has CAN-based OBDII (post-2008)
- Check CAN connections to OBDII port pins 6 (CAN-H) and 14 (CAN-L)
- Some vehicles use different CAN buses (powertrain vs chassis)

---

## Common Configurations

### Configuration 1: Track Car with Haltech

```cpp
// config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;
uint8_t SPEED_SOURCE = 1;  // Use CAN speed from wheel speeds
```

**Result:** All parameters, intelligent wheel speed averaging, perfect for track use.

### Configuration 2: Street Car with Megasquirt

```cpp
// config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_MEGASQUIRT;
uint8_t SPEED_SOURCE = 1;  // Use VSS1 from Megasquirt
```

**Result:** Core parameters plus vehicle speed, great for street/performance builds.

### Configuration 3: Data Logger with AiM

```cpp
// config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_AIM;
uint8_t SPEED_SOURCE = 1;  // Use speed from AiM
```

**Result:** All parameters, minimal CAN traffic, ideal for motorsport data logging.

### Configuration 4: Stock ECU with OBDII

```cpp
// config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_OBDII;
uint8_t SPEED_SOURCE = 3;  // Use GPS (OBDII speed available but GPS may be better)
```

**Result:** Basic parameters from stock ECU, works on any modern vehicle.

### Configuration 5: GPS for Speed (Any Protocol)

```cpp
// config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;  // Or any protocol
uint8_t SPEED_SOURCE = 3;  // Use GPS for speed
```

**Result:** Get engine data from CAN, accurate speed from GPS.

---

## Advanced Setup

### Custom ECU Message IDs

If your ECU uses non-standard message IDs:

1. Open `gauge_V4/can.cpp`
2. Find the `configureCANFilters()` function
3. Modify the filter configuration for your protocol
4. Example - accept custom Haltech IDs 0x400-0x40F:

```cpp
case CAN_PROTOCOL_HALTECH_V2:
  CAN0.init_Mask(0, 0, 0x7F0);
  CAN0.init_Filt(0, 0, 0x400);  // Add this line for 0x400-0x40F
  // ... rest of filters
  break;
```

### Disable Hardware Filtering (Debug Mode)

If you need to see all CAN traffic for debugging:

1. Open `gauge_V4/can.cpp`
2. Find `configureCANFilters()`
3. At the top, add:

```cpp
void configureCANFilters()
{
  // DEBUG: Accept all messages
  CAN0.init_Mask(0, 0, 0x00000000);
  CAN0.init_Mask(1, 0, 0x00000000);
  return;  // Skip protocol-specific filtering
  
  // ... rest of function
}
```

**Note:** This will increase CPU load significantly on busy CAN buses.

---

## Enable Debug Output

To see raw CAN messages:

1. Open `gauge_V4/can.cpp`
2. Find the `receiveCAN()` function (around line 62)
3. Uncomment the debug code (lines 74-94)
4. Upload firmware
5. Open Serial Monitor at 115200 baud

**You'll see:**
```
Standard ID: 0x360  DLC: 8  Data: 0x12 0x34 0x56 0x78 0x9A 0xBC 0xDE 0xF0
Standard ID: 0x361  DLC: 8  Data: 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88
```

**Use this to:**
- Verify ECU is sending data
- Check which message IDs are being broadcast
- Confirm data format (Big vs Little Endian)
- Identify custom message IDs

---

## Performance Tips

### Maximizing Update Rate

**Best to Worst:**
1. **AiM** - Fewest CAN messages, most efficient
2. **Haltech** - Fast broadcasts, comprehensive data
3. **Megasquirt** - Good broadcast rate, fewer parameters
4. **OBDII** - Slowest (polling-based)

### Reducing CAN Traffic

If you don't need certain parameters:

**Haltech:** Disable unused broadcasts in NSP
**Megasquirt:** Disable unused messages in TunerStudio  
**AiM:** Usually minimal anyway
**OBDII:** Can't disable (only what you poll)

### Multiple CAN Devices

If you have multiple devices on the CAN bus:
- Ensure no message ID conflicts
- Keep total bus load under 80%
- Monitor for message errors
- Consider using different CAN buses if available

---

## Specifications

### CAN Bus Requirements

**Physical Layer:**
- ISO 11898 CAN 2.0B
- Twisted pair cable
- 120Ω termination at each end
- Max cable length: 40m at 1 Mbps, 100m at 500 kbps

**Electrical:**
- CAN-H: 3.5V (dominant), 2.5V (recessive)
- CAN-L: 1.5V (dominant), 2.5V (recessive)
- Differential voltage: 2V (dominant)

**Supported Speeds:**
- 500 kbps (default for most protocols)
- 1000 kbps (some AiM configurations)
- Other speeds require code modification

### Protocol Timing

**Haltech v2:**
- Broadcast rate: 10-50 Hz per message
- No polling required

**Megasquirt:**
- Broadcast rate: 10-50 Hz per message
- No polling required

**AiM:**
- Broadcast rate: 10-100 Hz per message
- No polling required

**OBDII:**
- Priority 1 polling: 10 Hz (each parameter rotates)
- Priority 2 polling: 1 Hz
- Response time: ~10-50ms per query

---

## Frequently Asked Questions

### Q: Can I use multiple protocols at once?

**A:** No. The system is configured for one protocol at a time. Choose the one that matches your ECU.

### Q: Do I need to change anything besides CAN_PROTOCOL?

**A:** Usually no. Hardware filters configure automatically. You may want to set `SPEED_SOURCE = 1` to use CAN for speed.

### Q: Will this work with my ECU?

**A:** If your ECU is Haltech, Megasquirt, AiM, or OBDII-compliant, yes! For other ECUs, check if they use a compatible protocol format.

### Q: Can I add support for another ECU type?

**A:** Yes! See the Developer Guide for details on adding new protocols. It's typically ~100 lines of code per protocol.

### Q: What if my ECU uses different message IDs?

**A:** You can modify the filters in `can.cpp`. See the "Custom ECU Message IDs" section above.

### Q: Does hardware filtering affect functionality?

**A:** No! It's completely transparent. Your gauges work exactly the same, just more efficiently.

### Q: My ECU has custom firmware with different CAN format. What now?

**A:** You'll need to modify the protocol parser for your specific format. Check your ECU's CAN documentation and refer to the Developer Guide.

---

## Getting Help

### Things to Check First

1. ✓ CAN_PROTOCOL matches ECU type
2. ✓ CAN bus wiring correct
3. ✓ 120Ω termination installed
4. ✓ ECU CAN broadcasts enabled
5. ✓ Serial output shows "MCP2515 Initialized Successfully"
6. ✓ Serial output shows "CAN filters configured"

### Information to Provide

If asking for help, include:
- ECU type and model
- CAN_PROTOCOL setting
- Serial monitor output (full boot sequence)
- Which parameters work/don't work
- CAN debug output (if enabled)

### Resources

- **Developer Guide:** `documentation/CAN_DEVELOPER_GUIDE.md`
- **Protocol specs:** See references in Developer Guide
- **GitHub Issues:** Open an issue with details above

---

## Summary

✅ **Simple:** Change one variable to switch protocols  
✅ **Automatic:** Hardware filtering configures itself  
✅ **Fast:** 98% reduction in CPU overhead on busy buses  
✅ **Flexible:** Works with 4 major ECU types  
✅ **Reliable:** Proven hardware filtering and protocol support

**You're all set!** Choose your protocol, upload firmware, and enjoy professional-grade CAN bus integration with your custom gauge system.
