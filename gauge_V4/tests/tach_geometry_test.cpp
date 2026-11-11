#include "../tach_settings.h"
#include "../tach_preview.h"
#include <assert.h>

int main() {
  for (uint8_t count = 1; count <= 64; ++count) {
    for (uint8_t sweep = 0; sweep < 4; ++sweep) {
      uint8_t steps = tachSteps(count, sweep);
      assert(steps == (sweep < 2 ? count : (count + 1) / 2));
      for (uint8_t i = 0; i < count; ++i) {
        uint8_t step = tachStepForLed(i, count, sweep);
        assert(step < steps);
        if (sweep >= 2) assert(step == tachStepForLed(count - 1 - i, count, sweep));
      }
      assert(tachLitSteps(2999, 3000, 6000, steps) == 0);
      assert(tachLitSteps(3000, 3000, 6000, steps) == 1);
      assert(tachLitSteps(6000, 3000, 6000, steps) == steps);
      if (steps > 1) {
        for (uint8_t step = 1; step < steps; ++step) {
          uint16_t rpm = 3000 + ((uint32_t)step * 3000 + steps - 2) / (steps - 1);
          assert(tachLitSteps(rpm, 3000, 6000, steps) == step + 1);
          assert(tachLitSteps(rpm - 1, 3000, 6000, steps) == step);
        }
      }
    }
  }
  assert(tachStepForLed(0, 5, TACH_LEFT_TO_RIGHT) == 0);
  assert(tachStepForLed(0, 5, TACH_RIGHT_TO_LEFT) == 4);
  assert(tachStepForLed(0, 5, TACH_EDGE_TO_CENTER) == 0);
  assert(tachStepForLed(2, 5, TACH_EDGE_TO_CENTER) == 2);
  assert(tachStepForLed(2, 5, TACH_CENTER_TO_EDGE) == 0);
  assert(tachLitSteps(4000, 4000, 4000, 3) == 3);
  TachRecord record = {1, 27, 6, 2, TACH_EDGE_TO_CENTER, 3000, 6000, 0};
  record.checksum = tachChecksum(record);
  assert(tachRecordValid(record, 64));
  record.count ^= 1;
  assert(!tachRecordValid(record, 64));
  record.count = 1;
  record.checksum = tachChecksum(record);
  assert(!tachRecordValid(record, 64));
  TachColorRecord colors = {1, {30, 15, 0, 80, 10, 0, 80, 0, 0}, 0};
  colors.checksum = tachChecksum(colors);
  assert(tachColorRecordValid(colors));
  colors.colors[0] ^= 1;
  assert(!tachColorRecordValid(colors));
  TachPreview preview;
  assert(tachParsePreview("1B206021E0F00500A00500000", preview));
  assert(preview.count == 27 && preview.sweep == 2 && preview.warning == 6 && preview.shift == 2);
  assert(preview.normalColor == 0x1E0F00 && preview.warningColor == 0x500A00 && preview.shiftColor == 0x500000);
  assert(!tachParsePreview("1B206021E0F00500A0050000Z", preview));
  assert(!tachParsePreview("1B206021E0F00500A005000", preview));
  assert(!tachParsePreview("00206021E0F00500A00500000", preview));
  assert(!tachParsePreview("1B201061E0F00500A00500000", preview));
  assert(!tachPreviewExpired(5999, 1000));
  assert(tachPreviewExpired(6000, 1000));
  assert(tachPreviewExpired(4999, UINT32_MAX));
  TachFlashRecord flash = {1, 7200, 0};
  flash.checksum = tachChecksum(flash);
  assert(tachFlashRecordValid(flash));
  assert(tachFlashOrMaximum(flash, 6000) == 7200);
  flash.rpm ^= 1;
  assert(!tachFlashRecordValid(flash));
  flash = {0xFF, 0xFFFF, 0xFF};
  assert(tachFlashOrMaximum(flash, 6000) == 6000);
  flash = {1, 15001, 0};
  flash.checksum = tachChecksum(flash);
  assert(!tachFlashRecordValid(flash));
  assert(tachLitSteps(5999, 3000, 6000, 5) == 4);
  assert(tachLitSteps(6000, 3000, 6000, 5) == 5);
  assert(!tachFlashBlank(6999, 7000, 0, 1, false, false));
  assert(tachFlashBlank(7000, 7000, 0, 1, false, false));
  assert(!tachFlashBlank(7000, 7000, 2, 1, false, false));
  assert(!tachFlashBlank(7000, 7000, 0, 1, false, true));
  assert(!tachFlashBlank(7000, 7000, 0, 1, true, false));
  assert(tachLitSteps(5000, 3000, 6000, 5) == 3);
  assert(tachFlashBlank(5000, 5000, 1, 1, false, false));
}
