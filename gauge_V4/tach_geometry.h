#ifndef TACH_GEOMETRY_H
#define TACH_GEOMETRY_H

#include <stdint.h>

enum TachSweep : uint8_t {
  TACH_LEFT_TO_RIGHT = 0,
  TACH_RIGHT_TO_LEFT = 1,
  TACH_EDGE_TO_CENTER = 2,
  TACH_CENTER_TO_EDGE = 3
};

inline uint8_t tachSteps(uint8_t count, uint8_t sweep) {
  return sweep < TACH_EDGE_TO_CENTER ? count : (count + 1) / 2;
}

inline uint8_t tachStepForLed(uint8_t index, uint8_t count, uint8_t sweep) {
  if (sweep == TACH_LEFT_TO_RIGHT) return index;
  if (sweep == TACH_RIGHT_TO_LEFT) return count - 1 - index;
  uint8_t edge = index < count - 1 - index ? index : count - 1 - index;
  return sweep == TACH_EDGE_TO_CENTER ? edge : tachSteps(count, sweep) - 1 - edge;
}

inline uint8_t tachLitSteps(uint16_t rpm, uint16_t minimum, uint16_t maximum, uint8_t steps) {
  if (rpm < minimum) return 0;
  if (rpm >= maximum || minimum >= maximum || steps <= 1) return steps;
  return 1 + ((uint32_t)(rpm - minimum) * (steps - 1)) / (maximum - minimum);
}

inline bool tachFlashBlank(uint16_t rpm, uint16_t flashRpm, uint8_t fromEnd,
                           uint8_t shift, bool preview, bool flashState) {
  return !preview && rpm >= flashRpm && fromEnd <= shift && !flashState;
}

#endif
