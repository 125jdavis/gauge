/*
 * ========================================
 * SPLASH SCREEN SUPPORT
 * ========================================
 *
 * Each OLED shows a splash image at power-up and shutdown. SPLASH1_IMG /
 * SPLASH2_IMG (config_calibration.h) select it by splash image ID:
 *   0 .. SPLASH_IMAGE_COUNT-1 : built-in image (image_data.cpp SPLASH_IMAGES)
 *   SPLASH_CUSTOM_1 / _2      : user-uploaded image in EEPROM slot 1 / 2
 * A custom slot that holds no valid image falls back to that display's default.
 *
 * Custom images use the Adafruit drawBitmap layout: 32 rows of 16 bytes,
 * MSB = leftmost pixel, 1 = lit.
 *
 * EEPROM layout (CONFIG_TOOL_SPECIFICATION.md §10):
 *   Byte 509 : slot-1 validity flag (0xA5 = valid)
 *   Byte 510 : slot-2 validity flag (0xA5 = valid)
 *   Bytes 512–1023 : custom splash image slot 1  (512 bytes)
 *   Bytes 1024–1535: custom splash image slot 2  (512 bytes)
 *
 * Serial protocol (§3 – Splash Screen Upload):
 *   splash begin <slot>        -> "ready"   (invalidates the slot until "end" succeeds)
 *   splash data <hex64>        -> "ok" | "err"   (32 bytes per chunk)
 *   splash end <crc32_hex>     -> "ok" | "err: crc"
 *   splash test <slot>         -> "ok"      (shows custom slot N on display N)
 *   splash show <disp> <id>    -> "ok"      (shows splash image ID on display 1 or 2)
 *   splash clear <slot>        -> "ok"
 * test/show hold the image on screen for SPLASH_PREVIEW_MS.
 */

#ifndef SPLASH_H
#define SPLASH_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

/**
 * processSplashCommand - Handle a "splash …" serial command.
 *
 * @param cmd  Command text after "splash " (e.g. "begin 1")
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
 * splashIdValid - Return true if *id* is a built-in image or a custom slot.
 */
bool splashIdValid(uint8_t id);

/**
 * drawSplash - Draw splash image *id* on *display*.
 * An empty custom slot or unknown ID draws built-in image *fallback* instead.
 */
void drawSplash(Adafruit_SSD1306 *display, uint8_t id, uint8_t fallback);

/**
 * showSplashScreens - Draw the configured splash image on both displays.
 */
void showSplashScreens(void);

/**
 * splashPreviewActive - True while a "splash test/show" preview is on screen.
 * The main loop skips display updates meanwhile; when the hold ends this
 * forces both displays to redraw their normal screens.
 */
bool splashPreviewActive(void);

/**
 * crc32 - Compute CRC-32 (ISO 3309 / Ethernet polynomial 0xEDB88320).
 *
 * @param data  Pointer to data buffer
 * @param len   Number of bytes
 * @return CRC32 value
 */
uint32_t crc32(const uint8_t *data, size_t len);

#endif // SPLASH_H
