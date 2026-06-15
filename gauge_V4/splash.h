/*
 * ========================================
 * CUSTOM SPLASH SCREEN SUPPORT
 * ========================================
 *
 * Implements EEPROM storage, upload state machine, CRC32 verification,
 * and display functions for user-uploaded 128×32 monochrome splash images.
 *
 * EEPROM layout (CONFIG_TOOL_SPECIFICATION.md §10):
 *   Byte 509 : slot-1 validity flag (0xA5 = valid)
 *   Byte 510 : slot-2 validity flag (0xA5 = valid)
 *   Bytes 512–1023 : custom splash image slot 1  (512 bytes)
 *   Bytes 1024–1535: custom splash image slot 2  (512 bytes)
 *
 * Upload protocol (§3 – Splash Screen Upload):
 *   splash begin <id>          -> "ready"
 *   splash data <hex64>        -> "ok" | "err"   (32 bytes per chunk)
 *   splash end <crc32_hex>     -> "ok" | "err: crc"
 *   splash test <id>           -> "ok"
 *   splash clear <id>          -> "ok"
 */

#ifndef SPLASH_H
#define SPLASH_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

/**
 * processSplashCommand - Handle a "splash …" serial command.
 *
 * @param cmd  Full command string starting with "splash " (e.g. "splash begin 1")
 */
void processSplashCommand(const char *cmd);

/**
 * dispCustomSplash - Display a user-uploaded splash image from EEPROM.
 *
 * @param display  Pointer to Adafruit_SSD1306 instance
 * @param slot     1 or 2
 */
void dispCustomSplash(Adafruit_SSD1306 *display, uint8_t slot);

/**
 * isSplashValid - Return true if the given slot contains a validated image.
 *
 * @param slot  1 or 2
 */
bool isSplashValid(uint8_t slot);

/**
 * crc32 - Compute CRC-32 (ISO 3309 / Ethernet polynomial 0xEDB88320).
 *
 * @param data  Pointer to data buffer
 * @param len   Number of bytes
 * @return CRC32 value
 */
uint32_t crc32(const uint8_t *data, size_t len);

#endif // SPLASH_H
