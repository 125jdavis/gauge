/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * Handle CAN bus communication with Haltech ECU and other modules
 * CAN bus operates at 500kbps with standard 11-bit identifiers
 * 
 * Uses arduino-STM32-CAN library (nopnop2002)
 */

#ifndef CAN_H
#define CAN_H

#include <Arduino.h>

// ===== STM32 CAN FUNCTION DECLARATIONS =====
/**
 * canInit - Initialize STM32 native CAN controller
 */
bool canInit(uint32_t baudrate, uint8_t txPin, uint8_t rxPin);

/**
 * canReceive - Check if CAN message is available
 */
bool canReceive();

/**
 * canSend - Send CAN message using STM32 native CAN
 */
bool canSend(uint32_t id, uint8_t length, uint8_t *data);

// Define CAN baudrate constants for compatibility
#define CAN_500KBPS 500000
#define CAN_250KBPS 250000
#define CAN_1000KBPS 1000000

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

/**
 * parseCANHaltechV2 - Parse CAN messages using Haltech v2 protocol
 * 
 * Decodes messages according to Haltech CAN Broadcast Protocol V2.35.0
 * Supports: Vehicle Speed, Engine RPM, Coolant Temp, Fuel Pressure,
 * Oil Pressure, Oil Temperature, Lambda, Manifold Pressure
 * 
 * @param id - CAN message ID
 */
void parseCANHaltechV2(unsigned long id);

/**
 * parseCANMegasquirt - Parse CAN messages using Megasquirt protocol
 * 
 * Decodes messages according to Megasquirt CAN Broadcast protocol
 * Supports available parameters from the protocol spec
 * 
 * @param id - CAN message ID
 */
void parseCANMegasquirt(unsigned long id);

/**
 * parseCANAim - Parse CAN messages using AiM protocol
 * 
 * Decodes messages according to AiM CAN protocol
 * Supports available parameters from the protocol spec
 * 
 * @param id - CAN message ID
 */
void parseCANAim(unsigned long id);

/**
 * parseCANOBDII - Parse OBDII response messages
 * 
 * Decodes OBDII responses (ID 0x7E8-0x7EF) for polled PIDs
 * Supports: Vehicle Speed, Engine RPM, Coolant Temp, Lambda, Manifold Pressure
 * 
 * @param id - CAN message ID
 */
void parseCANOBDII(unsigned long id);

/**
 * sendOBDIIRequest - Send OBDII PID request
 * 
 * Sends a request to the ECU for a specific PID using standard OBDII protocol
 * Request is sent to ID 0x7DF (broadcast) or specific ECU ID
 * 
 * @param pid - Parameter ID to request (e.g., 0x0C for RPM, 0x0D for speed)
 */
void sendOBDIIRequest(uint8_t pid);

/**
 * pollOBDII - Manage OBDII polling for priority 1 and 2 parameters
 * 
 * Sends periodic requests for OBDII PIDs based on priority levels:
 * - Priority 1 (10Hz): Vehicle Speed, RPM, Lambda, Manifold Pressure
 * - Priority 2 (1Hz): Coolant Temp, Oil Temp
 * 
 * Should be called from main loop
 */
void pollOBDII();

/**
 * configureCANFilters - Configure MCP2515 hardware filters based on protocol
 * 
 * Sets up hardware acceptance filters and masks to reduce MCU processing load.
 * Only CAN messages matching the configured filters will trigger interrupts.
 * 
 * The MCP2515 has:
 * - 2 receive buffers (RXB0, RXB1)
 * - 6 acceptance filters (2 for RXB0, 4 for RXB1)
 * - 2 acceptance masks (1 for each buffer)
 * 
 * Filters are configured based on CAN_PROTOCOL setting to accept only
 * relevant message IDs for the selected ECU protocol.
 * 
 * Should be called after CAN0.begin() and before CAN0.setMode(MCP_NORMAL)
 */
void configureCANFilters();

#endif // CAN_H
