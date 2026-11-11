#ifndef TACH_PREVIEW_H
#define TACH_PREVIEW_H

#include "tach_geometry.h"
#include <string.h>

struct TachPreview {
  uint8_t count, sweep, warning, shift;
  uint32_t normalColor, warningColor, shiftColor;
};

inline bool tachPreviewExpired(uint32_t now, uint32_t lastUpdate) {
  return (uint32_t)(now - lastUpdate) >= 5000UL;
}

inline bool tachParseHex(const char *text, uint8_t length, uint32_t &value) {
  value = 0;
  for (uint8_t i = 0; i < length; ++i) {
    char c = text[i];
    uint8_t digit;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    else return false;
    value = (value << 4) | digit;
  }
  return true;
}

inline bool tachParsePreview(const char *text, TachPreview &preview) {
  if (strlen(text) != 25) return false;
  uint32_t count, sweep, warning, shift, normalColor, warningColor, shiftColor;
  if (!tachParseHex(text, 2, count) || !tachParseHex(text + 2, 1, sweep) ||
      !tachParseHex(text + 3, 2, warning) || !tachParseHex(text + 5, 2, shift) ||
      !tachParseHex(text + 7, 6, normalColor) || !tachParseHex(text + 13, 6, warningColor) ||
      !tachParseHex(text + 19, 6, shiftColor)) return false;
  if (count < 1 || count > 64 || sweep > TACH_CENTER_TO_EDGE ||
      warning >= tachSteps(count, sweep) || shift > warning) return false;
  preview = {(uint8_t)count, (uint8_t)sweep, (uint8_t)warning, (uint8_t)shift,
             normalColor, warningColor, shiftColor};
  return true;
}

#endif
