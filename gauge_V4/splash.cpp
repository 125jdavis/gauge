/*
 * ========================================
 * CUSTOM SPLASH SCREEN SUPPORT
 * ========================================
 */

#include "splash.h"
#include "globals.h"
#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// CRC-32 (ISO 3309 / Ethernet poly 0xEDB88320)
// ---------------------------------------------------------------------------
uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

// ---------------------------------------------------------------------------
// isSplashValid
// ---------------------------------------------------------------------------
bool isSplashValid(uint8_t slot) {
    uint16_t flagAddr = (slot == 1) ? splashValidFlag1Address : splashValidFlag2Address;
    return EEPROM.read(flagAddr) == 0xA5;
}

// ---------------------------------------------------------------------------
// dispCustomSplash
// ---------------------------------------------------------------------------
void dispCustomSplash(Adafruit_SSD1306 *display, uint8_t slot) {
    if (!display) return;
    uint16_t addr = (slot == 1) ? customSplash1Address : customSplash2Address;
    uint8_t buf[512];
    for (int i = 0; i < 512; i++) {
        buf[i] = EEPROM.read(addr + i);
    }
    display->clearDisplay();
    display->drawBitmap(0, 0, buf, 128, 32, WHITE);
    display->display();
}

// ---------------------------------------------------------------------------
// Upload state machine
// ---------------------------------------------------------------------------
static struct {
    bool     active;          // Upload in progress
    uint8_t  slot;            // 1 or 2
    uint16_t bytesReceived;   // Bytes written so far (0–512)
} uploadState = {false, 0, 0};

// Convert two ASCII hex digits to a byte.
// @param valid  Set to false if either character is not a valid hex digit.
static uint8_t hexByte(char hi, char lo, bool &valid) {
    auto nibble = [](char c) -> int8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int8_t h = nibble(hi);
    int8_t l = nibble(lo);
    if (h < 0 || l < 0) { valid = false; return 0; }
    return (uint8_t)((h << 4) | l);
}

// ---------------------------------------------------------------------------
// processSplashCommand
// ---------------------------------------------------------------------------
void processSplashCommand(const char *cmd) {
    // cmd is the full command string starting after "splash " (7 chars).
    // Called from processSerialCommands() in utilities.cpp.

    // "begin <slot>"
    if (strncmp_P(cmd, PSTR("begin "), 6) == 0) {
        uint8_t slot = (uint8_t)atoi(cmd + 6);
        if (slot != 1 && slot != 2) {
            Serial.println(F("err: range"));
            return;
        }
        if (uploadState.active) {
            Serial.println(F("err: busy"));
            return;
        }
        uploadState.active        = true;
        uploadState.slot          = slot;
        uploadState.bytesReceived = 0;
        Serial.println(F("ready"));
        return;
    }

    // "data <hex64>"  (64 hex chars = 32 bytes per chunk)
    if (strncmp_P(cmd, PSTR("data "), 5) == 0) {
        if (!uploadState.active) {
            Serial.println(F("err: not ready"));
            return;
        }
        const char *hex = cmd + 5;
        uint8_t hexLen  = (uint8_t)strlen(hex);
        if (hexLen == 0 || hexLen % 2 != 0) {
            Serial.println(F("err"));
            return;
        }
        uint8_t chunkBytes = hexLen / 2;
        if (uploadState.bytesReceived + chunkBytes > 512) {
            Serial.println(F("err: range"));
            uploadState.active = false;
            return;
        }
        uint16_t base = (uploadState.slot == 1) ? customSplash1Address : customSplash2Address;
        for (uint8_t i = 0; i < chunkBytes; i++) {
            bool valid = true;
            uint8_t b = hexByte(hex[i * 2], hex[i * 2 + 1], valid);
            if (!valid) {
                Serial.println(F("err: type"));
                uploadState.active = false;
                return;
            }
            EEPROM.update(base + uploadState.bytesReceived + i, b);
        }
        uploadState.bytesReceived += chunkBytes;
        Serial.println(F("ok"));
        return;
    }

    // "end <crc32hex>"
    if (strncmp_P(cmd, PSTR("end "), 4) == 0) {
        if (!uploadState.active) {
            Serial.println(F("err: not ready"));
            return;
        }
        if (uploadState.bytesReceived != 512) {
            Serial.println(F("err: range"));
            uploadState.active = false;
            return;
        }
        // Parse expected CRC32
        uint32_t expectedCRC = (uint32_t)strtoul(cmd + 4, nullptr, 16);
        // Read back from EEPROM and verify
        uint16_t base = (uploadState.slot == 1) ? customSplash1Address : customSplash2Address;
        uint8_t buf[512];
        for (int i = 0; i < 512; i++) buf[i] = EEPROM.read(base + i);
        uint32_t actualCRC = crc32(buf, 512);
        uploadState.active = false;
        if (actualCRC != expectedCRC) {
            Serial.println(F("err: crc"));
            return;
        }
        // Mark slot as valid
        uint16_t flagAddr = (uploadState.slot == 1) ? splashValidFlag1Address : splashValidFlag2Address;
        EEPROM.update(flagAddr, 0xA5);
        Serial.println(F("ok"));
        return;
    }

    // "test <slot>"
    if (strncmp_P(cmd, PSTR("test "), 5) == 0) {
        uint8_t slot = (uint8_t)atoi(cmd + 5);
        if (slot != 1 && slot != 2) {
            Serial.println(F("err: range"));
            return;
        }
        if (!isSplashValid(slot)) {
            Serial.println(F("err: unknown"));
            return;
        }
        dispCustomSplash(&display1, slot);
        Serial.println(F("ok"));
        return;
    }

    // "clear <slot>"
    if (strncmp_P(cmd, PSTR("clear "), 6) == 0) {
        uint8_t slot = (uint8_t)atoi(cmd + 6);
        if (slot != 1 && slot != 2) {
            Serial.println(F("err: range"));
            return;
        }
        uint16_t flagAddr = (slot == 1) ? splashValidFlag1Address : splashValidFlag2Address;
        EEPROM.update(flagAddr, 0x00);
        uploadState.active = false;
        Serial.println(F("ok"));
        return;
    }

    Serial.println(F("err: unknown"));
}
