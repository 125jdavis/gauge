#include "../ema_filter.h"

static int runTests()
{
    if (ema7Step(1000 * 128L, 300 * 128L, 0) != 1000 * 128L) return 1;
    if (ema7Step(1000 * 128L, 0, 64) != 500 * 128L) return 2;
    if (ema7Step(128, 0, 127) != 1) return 3;
    if (ema7Step(-1000 * 128L, 1000 * 128L, 64) != 0) return 4;
    if (ema7Step(65535L * 128, 0, 0) != 65535L * 128) return 5;
    if (ema7Step(32767L * 128, -32768L * 128, 0) != 32767L * 128) return 6;
    if (ema7StepWide(0xffffffffULL * 128, 0, 0) != 0xffffffffULL * 128) return 11;
    if (ema7StepWide(128, 1, 127) != 2) return 12;

    int32_t small = 0;
    for (int i = 0; i < 128; ++i) small = ema7Step(128, small, 127);
    if (small != 128) return 13;

    int32_t state = 0;
    for (int i = 0; i < 2000; ++i) {
        int32_t next = ema7Step(1000 * 128L, state, 127);
        if (next < state || next > 1000 * 128L) return 7;
        state = next;
    }
    if (state < 999 * 128L) return 8;
    for (int i = 0; i < 2000; ++i) {
        int32_t next = ema7Step(0, state, 127);
        if (next > state || next < 0) return 9;
        state = next;
    }
    if (state != 0) return 10;
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
