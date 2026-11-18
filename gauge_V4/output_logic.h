#ifndef OUTPUT_LOGIC_H
#define OUTPUT_LOGIC_H

// Platform-free logic for the low side outputs (ULN2003 channels on D12/D13).
// Kept free of Arduino headers so it can be unit tested (tests/output_logic_test.cpp).

#include <stdint.h>

constexpr uint8_t  LSO_COUNT       = 2;    // Output 1 (D12), Output 2 (D13)
constexpr uint8_t  LSO_PWM_POINTS  = 8;    // Breakpoints in the PWM curve
constexpr uint16_t LSO_FREQ_MIN_HZ = 20;
constexpr uint16_t LSO_FREQ_MAX_HZ = 500;

// Output modes
constexpr uint8_t LSO_MODE_DISABLED = 0;   // Output held off
constexpr uint8_t LSO_MODE_ON_OFF   = 1;   // Switched with hysteresis
constexpr uint8_t LSO_MODE_PWM      = 2;   // Duty from 8-point curve
constexpr uint8_t LSO_MODE_COUNT    = 3;

// On operator. The off operator is always the opposite one.
constexpr uint8_t LSO_ON_ABOVE = 0;        // On when value > on, off when value < off
constexpr uint8_t LSO_ON_BELOW = 1;        // On when value < on, off when value > off

// Hysteresis switch. Between the thresholds the previous state is held.
// If the thresholds are illogical (tool prevents this) the on condition wins.
inline bool lsoHysteresisStep(bool state, uint8_t onOp, long onVal, long offVal, long value)
{
    if (onOp == LSO_ON_BELOW) {
        if (value < onVal) return true;
        if (value > offVal) return false;
    } else {
        if (value > onVal) return true;
        if (value < offVal) return false;
    }
    return state;
}

// Duty in tenths of a percent (0-1000), linear between breakpoints and clamped at the ends.
// x must be strictly ascending (the tool enforces this); flat segments return the right-hand duty.
inline uint16_t lsoPwmDutyPermille(const int16_t *x, const uint8_t *duty, long value)
{
    if (value <= x[0]) return (uint16_t)duty[0] * 10;
    for (uint8_t i = 0; i + 1 < LSO_PWM_POINTS; i++) {
        if (value < x[i + 1]) {
            long span = (long)x[i + 1] - x[i];
            if (span <= 0) return (uint16_t)duty[i + 1] * 10;
            long d0 = (long)duty[i] * 10;
            long d1 = (long)duty[i + 1] * 10;
            // Max |(value - x)| * |d1 - d0| = 65535 * 1000, fits in 32 bits
            return (uint16_t)(d0 + ((value - x[i]) * (d1 - d0)) / span);
        }
    }
    return (uint16_t)duty[LSO_PWM_POINTS - 1] * 10;
}

// Timer TOP for a fast PWM period: f = timerHz / (TOP + 1). Frequency is clamped to 20-500 Hz.
inline uint16_t lsoPwmTop(uint32_t timerHz, uint16_t freqHz)
{
    if (freqHz < LSO_FREQ_MIN_HZ) freqHz = LSO_FREQ_MIN_HZ;
    if (freqHz > LSO_FREQ_MAX_HZ) freqHz = LSO_FREQ_MAX_HZ;
    return (uint16_t)((timerHz + freqHz / 2) / freqHz - 1);
}

// Compare value for a duty (0-1000 permille); high time is (compare + 1) ticks.
// Caller handles 0 duty by disconnecting the pin (compare 0 still gives a 1-tick pulse).
inline uint16_t lsoPwmCompare(uint16_t top, uint16_t permille)
{
    if (permille >= 1000) return top;
    uint32_t high = ((uint32_t)(top + 1UL) * permille + 500) / 1000;
    return high == 0 ? 0 : (uint16_t)(high - 1);
}

#endif // OUTPUT_LOGIC_H
