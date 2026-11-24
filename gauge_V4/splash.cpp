/*
 * ========================================
 * SPLASH SCREEN SUPPORT
 * ========================================
 */

#include "splash.h"
#include "globals.h"
#include "config_hardware.h"
#include "config_calibration.h"
#include "image_data.h"
#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// CRC-32 (ISO 3309 / Ethernet poly 0xEDB88320)
// ---------------------------------------------------------------------------
static uint32_t crc32Update(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (uint8_t j = 0; j < 8; j++) {
        crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return crc;
}

uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) crc = crc32Update(crc, data[i]);
    return ~crc;
}

static uint16_t slotAddress(uint8_t slot) {
    return (slot == 1) ? customSplash1Address : customSplash2Address;
}

static uint16_t slotFlagAddress(uint8_t slot) {
    return (slot == 1) ? splashValidFlag1Address : splashValidFlag2Address;
}

// ---------------------------------------------------------------------------
// isSplashValid
// ---------------------------------------------------------------------------
bool isSplashValid(uint8_t slot) {
    return EEPROM.read(slotFlagAddress(slot)) == 0xA5;
}

// ---------------------------------------------------------------------------
// dispCustomSplash - drawBitmap layout (16 bytes per row, MSB = left pixel),
// read straight from EEPROM so no 512-byte RAM buffer is needed.
// ---------------------------------------------------------------------------
void dispCustomSplash(Adafruit_SSD1306 *display, uint8_t slot) {
    if (!display) return;
    uint16_t addr = slotAddress(slot);
    display->clearDisplay();
    for (uint8_t y = 0; y < SCREEN_H; y++) {
        for (uint8_t xb = 0; xb < SCREEN_W / 8; xb++) {
            uint8_t b = EEPROM.read(addr++);
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (b & (0x80 >> bit)) display->drawPixel(xb * 8 + bit, y, WHITE);
            }
        }
    }
    display->display();
}

// ---------------------------------------------------------------------------
// Splash selection
// ---------------------------------------------------------------------------
bool splashIdValid(uint8_t id) {
    return id < SPLASH_IMAGE_COUNT || id == SPLASH_CUSTOM_1 || id == SPLASH_CUSTOM_2;
}

void drawSplash(Adafruit_SSD1306 *display, uint8_t id, uint8_t fallback) {
    if (!display) return;
    if (id == SPLASH_CUSTOM_1 || id == SPLASH_CUSTOM_2) {
        uint8_t slot = id - SPLASH_CUSTOM_1 + 1;
        if (isSplashValid(slot)) {
            dispCustomSplash(display, slot);
            return;
        }
        id = fallback;
    }
    if (id >= SPLASH_IMAGE_COUNT) id = fallback;
    const unsigned char *img = (const unsigned char *)pgm_read_ptr(&SPLASH_IMAGES[id]);
    display->clearDisplay();
    display->drawBitmap(0, 0, img, SCREEN_W, SCREEN_H, WHITE);
    display->display();
}

void showSplashScreens(void) {
    drawSplash(&display1, SPLASH1_IMG, SPLASH1_DEFAULT);
    drawSplash(&display2, SPLASH2_IMG, SPLASH2_DEFAULT);
}

// ---------------------------------------------------------------------------
// Preview hold for "splash test" / "splash show"
// ---------------------------------------------------------------------------
static bool previewHold = false;
static unsigned long previewStart = 0;

static void startPreview(void) {
    previewHold = true;
    previewStart = millis();
}

bool splashPreviewActive(void) {
    if (!previewHold) return false;
    if (millis() - previewStart < SPLASH_PREVIEW_MS) return true;
    previewHold = false;
    // Force both displays to fully redraw their normal screens.
    dispArray1_prev[0] = 255;
    dispArray2_prev = 255;
    staticContentDrawn1 = false;
    staticContentDrawn2 = false;
    return false;
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
        // The slot is invalid until "end" verifies the new image, so a failed or
        // abandoned upload falls back to the built-in default image.
        EEPROM.update(slotFlagAddress(slot), 0x00);
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
        uint16_t base = slotAddress(uploadState.slot);
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
        uint16_t base = slotAddress(uploadState.slot);
        uint32_t crc = 0xFFFFFFFF;
        for (int i = 0; i < 512; i++) crc = crc32Update(crc, EEPROM.read(base + i));
        uint32_t actualCRC = ~crc;
        uploadState.active = false;
        if (actualCRC != expectedCRC) {
            Serial.println(F("err: crc"));
            return;
        }
        // Mark slot as valid
        EEPROM.update(slotFlagAddress(uploadState.slot), 0xA5);
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
        dispCustomSplash(slot == 1 ? &display1 : &display2, slot);
        startPreview();
        Serial.println(F("ok"));
        return;
    }

    // "show <display> <id>" - preview any splash image ID on display 1 or 2
    if (strncmp_P(cmd, PSTR("show "), 5) == 0) {
        char *rest;
        long disp = strtol(cmd + 5, &rest, 10);
        long id = strtol(rest, nullptr, 10);
        if ((disp != 1 && disp != 2) || id < 0 || id > 255 || !splashIdValid((uint8_t)id)) {
            Serial.println(F("err: range"));
            return;
        }
        drawSplash(disp == 1 ? &display1 : &display2, (uint8_t)id,
                   disp == 1 ? SPLASH1_DEFAULT : SPLASH2_DEFAULT);
        startPreview();
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
        EEPROM.update(slotFlagAddress(slot), 0x00);
        uploadState.active = false;
        Serial.println(F("ok"));
        return;
    }

    Serial.println(F("err: unknown"));
}
