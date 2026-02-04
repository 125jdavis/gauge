/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * Handle CAN bus communication with Haltech ECU and other modules
 * CAN bus operates at 500kbps with standard 11-bit identifiers
 * 
 * STM32 VERSION: Uses native CAN controller (not MCP2515)
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
 */
void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4);

/**
 * parseCAN - Parse received CAN message based on ID
 * 
 * Decodes CAN messages from various ECU protocols.
 * Updates global variables with decoded sensor values.
 * 
 * @param id - CAN message ID
 * @param msg - Reserved for future use
 */
void parseCAN(unsigned long id, unsigned long msg);

/**
 * parseCANHaltechV2 - Parse CAN messages using Haltech v2 protocol
 */
void parseCANHaltechV2(unsigned long id);

/**
 * parseCANMegasquirt - Parse CAN messages using Megasquirt protocol
 */
void parseCANMegasquirt(unsigned long id);

/**
 * parseCANAim - Parse CAN messages using AiM protocol
 */
void parseCANAim(unsigned long id);

/**
 * parseCANOBDII - Parse OBDII response messages
 */
void parseCANOBDII(unsigned long id);

/**
 * sendOBDIIRequest - Send OBDII PID request
 */
void sendOBDIIRequest(uint8_t pid);

/**
 * pollOBDII - Manage OBDII polling for priority 1 and 2 parameters
 */
void pollOBDII();

/**
 * configureCANFilters - Configure STM32 CAN hardware filters based on protocol
 * 
 * STM32 version: Uses native CAN peripheral filter banks
 * Sets up hardware acceptance filters to reduce MCU processing load.
 * Only CAN messages matching the configured filters will be received.
 */
void configureCANFilters();

#endif // CAN_H
