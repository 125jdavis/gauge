#ifndef EMA_FILTER_H
#define EMA_FILTER_H

#include <stdint.h>

// State and samples use seven fractional bits; retention is 0..127.
inline int32_t ema7Step(int32_t sample, int32_t state, uint8_t retention)
{
    int32_t delta = sample - state;
    int32_t step = (delta * (128 - (int32_t)retention)) >> 7;
    if (delta > 0 && step == 0) step = 1;
    return state + step;
}

inline uint64_t ema7StepWide(uint64_t sample, uint64_t state, uint8_t retention)
{
    uint64_t next = (sample * (128 - retention) + state * retention) >> 7;
    if (sample > state && next == state) ++next;
    return next;
}

#endif
