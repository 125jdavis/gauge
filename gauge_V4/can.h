/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * Handle CAN bus communication with Haltech ECU and other modules
 * CAN bus operates at 500kbps with standard 11-bit identifiers
 */

#ifndef CAN_H
#define CAN_H

#include <Arduino.h>

/**
 * sendCAN_LE - Send CAN message with Little Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Little Endian: Low byte first, then high byte (Intel byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x34 (low byte)
 *   data[1] = 0x12 (high byte)
 * 
 * Used for: Sensor data to other modules (thermistor, fuel level, baro)
 */
void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4);

/**
 * sendCAN_BE - Send CAN message with Big Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Big Endian: High byte first, then low byte (Motorola byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x12 (high byte)
 *   data[1] = 0x34 (low byte)
 * 
 * Used for: Speed data (compatible with Haltech protocol)
 */
void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4);

/**
 * receiveCAN - Read CAN message from receive buffer
 * 
 * Reads a CAN message from the MCP2515 controller's receive buffer.
 * Called when CAN interrupt pin goes low (message waiting).
 * Message data is copied to canMessageData[] for parsing.
 * 
 * Global variables modified:
 * - rxId: CAN message identifier
 * - len: Number of data bytes (0-8)
 * - rxBuf: Raw message data bytes
 * - canMessageData: Copy of message data for parsing
 * 
 * The commented-out debug code can be enabled to print CAN messages to serial.
 */
void receiveCAN();

/**
 * parseCAN - Parse received CAN message based on ID
 * 
 * Decodes CAN messages from Haltech ECU protocol and test messages.
 * Updates global variables with decoded sensor values.
 * 
 * @param id - CAN message ID
 * @param msg - CAN message data (not currently used, data is in rxBuf)
 * 
 * Supported message IDs:
 * - 0x301: Test pump pressure
 * - 0x360: RPM, MAP, TPS
 * - 0x361: Fuel pressure, oil pressure
 * - 0x362: Injector duty cycle, ignition angle
 * - 0x368: Air/Fuel Ratio
 * - 0x369: Knock level
 * - 0x3E0: Coolant temp, intake air temp, fuel temp, oil temp
 * - 0x3E1: Transmission temp, fuel composition
 * 
 * Global variables modified: Various CAN data variables (rpmCAN, mapCAN, etc.)
 */
void parseCAN(unsigned long id, unsigned long msg);

#endif // CAN_H
