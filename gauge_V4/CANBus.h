/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * This file contains functions for CAN bus communication
 * including sending, receiving, and parsing messages
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef CAN_BUS_H
#define CAN_BUS_H

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
 * receiveCAN - Read CAN message from receive buffer
 * 
 * Reads a CAN message from the MCP2515 controller's receive buffer.
 * Called when CAN interrupt pin goes low (message waiting).
 */
void receiveCAN();

/**
 * parseCAN - Parse received CAN message and extract data
 * 
 * Decodes Haltech ECU CAN messages based on message ID.
 * Extracts engine parameters from the 8-byte message payload.
 * 
 * @param id - CAN message identifier
 * @param msg - Pointer to 8-byte message data array
 */
void parseCAN(unsigned long id, unsigned long msg);

#endif // CAN_BUS_H
