# CAN Protocol Selection Guide

## Overview

The gauge system now supports multiple CAN protocols for ECU communication. This allows you to select the appropriate protocol based on your ECU manufacturer and model.

## Supported Protocols

The following protocols are currently supported:

1. **Haltech v2** (CAN_PROTOCOL_HALTECH_V2 = 0)
   - Reference: Haltech CAN Broadcast Protocol V2.35.0
   - Default protocol
   
2. **Megasquirt** (CAN_PROTOCOL_MEGASQUIRT = 1)
   - Reference: Megasquirt CAN Broadcast Protocol
   
3. **AiM** (CAN_PROTOCOL_AIM = 2)
   - Reference: AiM CAN Protocol
   
4. **OBDII** (CAN_PROTOCOL_OBDII = 3)
   - Standard OBDII protocol with active polling

## How to Select a Protocol

To select a CAN protocol, edit the `gauge_V4/config_calibration.cpp` file and modify the `CAN_PROTOCOL` variable:

```cpp
// In config_calibration.cpp

// For Haltech v2 (default):
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;

// For Megasquirt:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_MEGASQUIRT;

// For AiM:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_AIM;

// For OBDII:
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_OBDII;
```

After changing the protocol, upload the updated firmware to your Arduino.

**Note:** Hardware CAN message filters are automatically configured based on the selected protocol. This significantly reduces MCU interrupt load by filtering irrelevant messages at the hardware level. See `CAN_HARDWARE_FILTERING.md` for details.

## Monitored Parameters by Protocol

The system attempts to read the following parameters from each protocol:

### Available Parameters

| Parameter | Haltech v2 | Megasquirt | AiM | OBDII |
|-----------|-----------|------------|-----|-------|
| Vehicle Speed | ✓ | ✓ | ✓ | ✓ (polled) |
| Engine RPM | ✓ | ✓ | ✓ | ✓ (polled) |
| Coolant Temp | ✓ | ✓ | ✓ | ✓ (polled) |
| Fuel Pressure | ✓ | - | ✓ | - |
| Oil Pressure | ✓ | - | ✓ | - |
| Oil Temperature | ✓ | - | ✓ | - |
| Lambda (AFR) | ✓ | ✓ | ✓ | ✓ (polled) |
| Manifold Pressure | ✓ | ✓ | ✓ | ✓ (polled) |

**Note:** Parameters marked with "-" are not available in that protocol. The system will ignore unavailable parameters and continue to operate normally.

**Note on Vehicle Speed:** All protocols now support vehicle speed via CAN. Set `SPEED_SOURCE = 1` in config_calibration.cpp to use CAN for vehicle speed input.
- **Haltech v2**: Reads wheel speeds (0x470-0x473) and averages non-zero values
- **Megasquirt**: Reads VSS1 (0x5EC)
- **AiM**: Reads speed from standard message (0x0B0)
- **OBDII**: Polls speed via PID 0x0D at 10Hz

Alternatively, use GPS (`SPEED_SOURCE = 3`) or Hall sensor (`SPEED_SOURCE = 2`).

## Protocol-Specific Details

### Haltech v2 Protocol

- **Message Format:** Big Endian (MSB first)
- **Baud Rate:** 500 kbps
- **Broadcast Mode:** ECU broadcasts data automatically
- **Message IDs:**
  - 0x360: RPM, MAP, TPS
  - 0x361: Fuel Pressure, Oil Pressure
  - 0x362: Injector Duty, Ignition Angle
  - 0x368: Lambda (AFR)
  - 0x369: Knock Level
  - 0x3E0: Coolant Temp, Air Temp, Fuel Temp, Oil Temp
  - 0x3E1: Trans Temp, Fuel Composition
  - 0x470: Wheel Speed Front Left
  - 0x471: Wheel Speed Front Right
  - 0x472: Wheel Speed Rear Left
  - 0x473: Wheel Speed Rear Right (triggers average calculation)

**Note:** Vehicle speed is calculated by averaging non-zero wheel speeds. If all wheel speeds are zero, vehicle speed is set to zero.

### Megasquirt Protocol

- **Message Format:** Little Endian (LSB first)
- **Baud Rate:** 500 kbps
- **Broadcast Mode:** ECU broadcasts data automatically
- **Base ID:** 0x5F0 (configurable in ECU)
- **Message IDs:**
  - 0x5F0: MAP, RPM
  - 0x5F1: Coolant Temp
  - 0x5F2: TPS
  - 0x5F3: AFR
  - 0x5F4: Knock
  - 0x5EC: Vehicle Speed (VSS1)

**Note:** Oil pressure, oil temp, and fuel pressure are not standard in Megasquirt broadcasts. These would need to be configured as custom channels in your Megasquirt ECU if required.

### AiM Protocol

- **Message Format:** Big Endian (MSB first)
- **Baud Rate:** 1000 kbps (may vary based on ECU configuration)
- **Broadcast Mode:** ECU broadcasts data automatically
- **Message IDs:**
  - 0x0B0: RPM, Speed
  - 0x0B1: Coolant Temp, Oil Temp
  - 0x0B2: MAP, Oil Pressure, Fuel Pressure
  - 0x0B3: Lambda

**Note:** AiM message IDs may vary based on your specific ECU configuration. Consult your ECU manual for exact message mapping.

### OBDII Protocol

- **Message Format:** Standard OBDII format
- **Baud Rate:** 500 kbps (standard for CAN-based OBDII)
- **Polling Mode:** Active polling required (ECU does not broadcast)
- **Request ID:** 0x7DF (functional broadcast)
- **Response IDs:** 0x7E8 - 0x7EF
- **PIDs Polled:**
  - **Priority 1 (10 Hz):** Speed (0x0D), RPM (0x0C), Lambda (0x24), MAP (0x0B)
  - **Priority 2 (1 Hz):** Coolant Temp (0x05)

**Note:** 
- OBDII does not provide Oil Pressure, Oil Temperature, or Fuel Pressure via standard PIDs
- The polling strategy rotates through Priority 1 PIDs to avoid overwhelming the ECU
- Some vehicles may not support all PIDs (especially Lambda/0x24 on older vehicles)

## Message Filtering

The system automatically filters CAN messages based on the selected protocol:

- **Haltech v2:** Filters for message IDs 0x360-0x362, 0x368-0x369, 0x3E0-0x3E1
- **Megasquirt:** Filters for message IDs 0x5F0-0x5F4 (base 0x5F0)
- **AiM:** Filters for message IDs 0x0B0-0x0B3
- **OBDII:** Filters for response IDs 0x7E8-0x7EF

Messages not matching the protocol's expected IDs are ignored, reducing processing overhead.

## Troubleshooting

### No Data Received

1. **Check CAN bus wiring:**
   - Ensure CAN-H and CAN-L are properly connected
   - Verify 120Ω termination resistors are present
   
2. **Verify baud rate:**
   - Most ECUs use 500 kbps (current setting)
   - AiM may use 1000 kbps - adjust in code if needed
   
3. **Check protocol selection:**
   - Ensure `CAN_PROTOCOL` matches your ECU type
   
4. **Enable debug output:**
   - Uncomment the debug code in `can.cpp` `receiveCAN()` function to view raw messages

### Incorrect Values

1. **Byte order issue:**
   - Verify Big Endian vs Little Endian for your ECU
   - Some ECUs may use non-standard byte ordering
   
2. **Scaling factors:**
   - Check that the protocol implementation matches your ECU's scaling
   - Refer to your ECU's CAN protocol documentation
   
3. **Custom ECU configuration:**
   - Some ECUs allow customizing message IDs and formats
   - Verify your ECU is using the standard protocol configuration

### OBDII-Specific Issues

1. **Slow updates:**
   - OBDII polling is inherently slower than broadcast protocols
   - Priority 1 parameters update at 10 Hz (100ms per PID rotation)
   - Priority 2 parameters update at 1 Hz
   
2. **Missing parameters:**
   - Not all vehicles support all PIDs
   - Lambda (PID 0x24) is often not available on older vehicles
   - Consider using a different protocol if your ECU supports it

## Adding Custom Protocols

To add support for a custom protocol:

1. Add a new enum value to `CANProtocol` in `config_calibration.h`
2. Create a new parsing function in `can.cpp` (e.g., `parseCANCustom()`)
3. Add the new case to the switch statement in `parseCAN()`
4. Document the new protocol in this guide

Example:
```cpp
// In config_calibration.h
enum CANProtocol {
  // ... existing protocols ...
  CAN_PROTOCOL_CUSTOM = 4
};

// In can.cpp
void parseCANCustom(unsigned long id) {
  // Your custom parsing logic here
}

// In parseCAN()
case CAN_PROTOCOL_CUSTOM:
  parseCANCustom(id);
  break;
```

## References

- [Haltech CAN Protocol V2.35.0](https://www.ptmotorsport.com.au/wp-content/uploads/2022/09/Haltech-CAN-Broadcast-Protocol-V2.35.0-1.pdf)
- [Megasquirt CAN Broadcast](https://www.msextra.com/doc/pdf/Megasquirt_CAN_Broadcast.pdf)
- [AiM CAN Protocol](https://support.aimshop.com/downloads/ecu/aim/AiM_CAN.pdf)
- [OBDII PID Reference](https://en.wikipedia.org/wiki/OBD-II_PIDs)

## Support

For issues or questions about CAN protocol support, please refer to your ECU's documentation or open an issue in the project repository.
