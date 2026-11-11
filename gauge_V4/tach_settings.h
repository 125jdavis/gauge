#ifndef TACH_SETTINGS_H
#define TACH_SETTINGS_H

#include "tach_geometry.h"

#pragma pack(push, 1)
struct TachRecord {
  uint8_t version;
  uint8_t count;
  uint8_t warning;
  uint8_t shift;
  uint8_t sweep;
  uint16_t minimum;
  uint16_t maximum;
  uint8_t checksum;
};
struct TachColorRecord {
  uint8_t version;
  uint8_t colors[9];
  uint8_t checksum;
};
struct TachFlashRecord {
  uint8_t version;
  uint16_t rpm;
  uint8_t checksum;
};
#pragma pack(pop)

template <typename T>
inline uint8_t tachChecksum(const T &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint8_t checksum = 0xA7;
  for (uint8_t i = 0; i < sizeof(record) - 1; i++) checksum ^= bytes[i];
  return checksum;
}

inline bool tachColorRecordValid(const TachColorRecord &record) {
  return record.version == 1 && record.checksum == tachChecksum(record);
}

inline bool tachFlashRecordValid(const TachFlashRecord &record) {
  return record.version == 1 && record.checksum == tachChecksum(record) && record.rpm <= 15000;
}

inline uint16_t tachFlashOrMaximum(const TachFlashRecord &record, uint16_t maximum) {
  return record.version == 0 || record.version == 0xFF ? maximum : record.rpm;
}

inline bool tachRecordValid(const TachRecord &record, uint8_t maxLeds) {
  return record.version == 1 && record.checksum == tachChecksum(record) &&
         record.count >= 1 && record.count <= maxLeds && record.sweep <= TACH_CENTER_TO_EDGE &&
         record.warning < tachSteps(record.count, record.sweep) && record.shift <= record.warning &&
         record.minimum <= 5000 && record.maximum >= 1000 && record.maximum <= 15000 &&
         record.minimum <= record.maximum;
}

#endif
