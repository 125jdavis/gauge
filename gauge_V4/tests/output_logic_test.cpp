#include "../output_logic.h"

static int runTests()
{
    // Hysteresis, on above: on > 95, off < 90, hold in between
    if (lsoHysteresisStep(false, LSO_ON_ABOVE, 95, 90, 96) != true) return 1;
    if (lsoHysteresisStep(true,  LSO_ON_ABOVE, 95, 90, 92) != true) return 2;
    if (lsoHysteresisStep(false, LSO_ON_ABOVE, 95, 90, 92) != false) return 3;
    if (lsoHysteresisStep(true,  LSO_ON_ABOVE, 95, 90, 89) != false) return 4;
    if (lsoHysteresisStep(true,  LSO_ON_ABOVE, 95, 90, 90) != true) return 5;
    // On below: on < 11.5 V, off > 12.5 V (volts * 10)
    if (lsoHysteresisStep(false, LSO_ON_BELOW, 115, 125, 114) != true) return 6;
    if (lsoHysteresisStep(true,  LSO_ON_BELOW, 115, 125, 120) != true) return 7;
    if (lsoHysteresisStep(true,  LSO_ON_BELOW, 115, 125, 126) != false) return 8;

    // PWM curve: clamp at ends, interpolate, permille output
    const int16_t x[LSO_PWM_POINTS] = {60, 70, 75, 80, 85, 90, 95, 100};
    const uint8_t d[LSO_PWM_POINTS] = {0, 0, 20, 35, 50, 65, 80, 100};
    if (lsoPwmDutyPermille(x, d, -40) != 0) return 10;
    if (lsoPwmDutyPermille(x, d, 200) != 1000) return 11;
    if (lsoPwmDutyPermille(x, d, 75) != 200) return 12;
    if (lsoPwmDutyPermille(x, d, 77) != 260) return 13;      // 20 + 2/5 * 15 = 26 %
    if (lsoPwmDutyPermille(x, d, 99) != 960) return 14;
    // Falling curve and wide span stay in range
    const int16_t xw[LSO_PWM_POINTS] = {-32768, -1000, 0, 1, 2, 3, 4, 32767};
    const uint8_t dw[LSO_PWM_POINTS] = {100, 90, 80, 70, 60, 50, 40, 0};
    if (lsoPwmDutyPermille(xw, dw, 16383) != 201) return 15;  // ~half of 400->0 over 4..32767 (truncates)

    // Timer TOP at 250 kHz: 20 Hz -> 12499, 500 Hz -> 499, out of range clamps
    if (lsoPwmTop(250000UL, 20) != 12499) return 20;
    if (lsoPwmTop(250000UL, 500) != 499) return 21;
    if (lsoPwmTop(250000UL, 5) != 12499) return 22;
    if (lsoPwmTop(250000UL, 1000) != 499) return 23;
    if (lsoPwmCompare(499, 1000) != 499) return 24;
    if (lsoPwmCompare(499, 500) != 249) return 25;
    if (lsoPwmCompare(499, 1) != 0) return 26;
    return 0;
}

int main()
{
    int result = runTests();
#ifdef __AVR__
    *(volatile uint8_t *)0x3e = (uint8_t)(result + 1);
    for (;;) {}
#else
    return result;
#endif
}
