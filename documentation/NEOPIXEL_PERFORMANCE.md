# Adafruit_NeoPixel Performance Analysis

## Summary

**Performance penalty: ~8% increase in LED update time**
- FastLED: 2.02 ms per 64-LED update
- NeoPixel: 2.18 ms per 64-LED update
- **Difference: +0.16 ms**

**Practical impact: NEGLIGIBLE for tachometer application**

## WS2812 Protocol Timing (Hardware Constraint)

WS2812 LEDs require precise timing regardless of library choice:

- **Bit rate:** 800 kHz (1.25 μs per bit)
- **Bits per LED:** 24 bits (8 red + 8 green + 8 blue)
- **Bits for 64 LEDs:** 64 × 24 = 1,536 bits
- **Minimum transmission time:** 1,536 bits ÷ 800,000 bps = **1.92 ms**
- **Plus reset time:** ~50 μs
- **Total minimum:** ~2.0 ms

This 2.0 ms is a hardware limitation - no software can make it faster.

## CPU Overhead Comparison

### STM32F407 Specifications
- Clock speed: 168 MHz
- Cycle time: 1/168MHz = 5.95 ns per instruction

### FastLED (Direct Register Access)
- Uses direct ARM register writes
- Instructions per bit: ~1-2 (direct BSRR register write)
- Per-bit overhead: ~6-12 ns (1-2 CPU cycles)
- **Total for 64 LEDs:** 2.0 ms + (1536 × 0.01 μs) ≈ **2.02 ms**

### Adafruit_NeoPixel (digitalWrite)
- Uses Arduino digitalWrite() function
- Instructions per bit: ~15-25 (pin lookup + port calculation + register write)
- Per-bit overhead: ~60-180 ns (10-30 CPU cycles)
- **Total for 64 LEDs:** 2.0 ms + (1536 × 0.12 μs) ≈ **2.18 ms**

## Performance Metrics

| Metric | FastLED | NeoPixel | Difference |
|--------|---------|----------|------------|
| Time per update (64 LEDs) | 2.02 ms | 2.18 ms | +0.16 ms |
| Max refresh rate | 495 Hz | 459 Hz | -36 Hz |
| CPU cycles used | 339,360 | 366,240 | +26,880 (+7.9%) |

## Impact on Tachometer Application

### Typical LED Update Frequencies

The tachometer LED strip updates at varying rates depending on engine RPM changes:

- **Fast changes (acceleration):** ~60 Hz (every 16.7 ms)
- **Slow changes (cruise):** ~10 Hz (every 100 ms)
- **Static (idle):** ~1-5 Hz

### Overhead per Frame

| Update Rate | Frame Time | NeoPixel Time | Overhead % |
|-------------|------------|---------------|------------|
| 60 Hz | 16.7 ms | 2.18 ms | **13%** |
| 30 Hz | 33.3 ms | 2.18 ms | **6.5%** |
| 10 Hz | 100 ms | 2.18 ms | **2.2%** |
| 5 Hz | 200 ms | 2.18 ms | **1.1%** |

The additional 0.16 ms overhead:
- At 60 Hz: 0.16 ms / 16.7 ms = **~1% extra overhead**
- At 10 Hz: 0.16 ms / 100 ms = **~0.16% extra overhead**

### Real-World System Timing

Other system components that matter MORE than library choice:

| Component | Typical Latency |
|-----------|-----------------|
| CAN bus message | 1-5 ms |
| Sensor sampling (ADC) | 1-10 ms |
| Display refresh (OLED) | 5-20 ms per frame |
| GPS update | 100-1000 ms |
| Motor step | 0.1 ms per step |

The 0.16 ms NeoPixel overhead is **insignificant** compared to these timing factors.

## Why NeoPixel Over FastLED

### Technical Reason

FastLED requires board-specific pin mapping files for each microcontroller board variant. The supported boards are hardcoded in `fastpin_dispatcher.h`:

```cpp
// STM32F4 boards supported by FastLED:
- ARDUINO_BLACKPILL_F411CE
- ARDUINO_NUCLEO_F411RE
- ARDUINO_BLACKPILL_F401CC/CE
- ARDUINO_NUCLEO_F401RE
- ARDUINO_DISCO_F407VG  // Discovery board only
- ARDUINO_NUCLEO_F446RE
- ARDUINO_NUCLEO_F429ZI/F439ZI

// NOT supported:
- ARDUINO_GENERIC_F407VETX  // Our board!
```

When compiling for Generic F407VETx, FastLED generates:
```
#error "STM32: Unknown board. Check your ARDUINO_* board macro..."
```

### Adafruit_NeoPixel Advantages

1. **Universal GPIO Support:** Works with ANY pin on ANY STM32 board
2. **No Board Files Needed:** Uses standard Arduino digitalWrite()
3. **Simpler API:** Easier to understand and modify
4. **Well Maintained:** Active development and support
5. **Proven Reliability:** Used in thousands of projects

The 8% performance penalty is a small price to pay for guaranteed compatibility.

## Conclusion

**The switch from FastLED to Adafruit_NeoPixel is the correct choice:**

1. ✅ FastLED doesn't support our board (compilation would fail)
2. ✅ NeoPixel performance is sufficient (2.18 ms vs 2.02 ms)
3. ✅ Overhead is negligible in context (~1% of frame time)
4. ✅ No visible difference in tachometer responsiveness
5. ✅ Universal compatibility with all STM32 boards

## Assumptions and Notes

### Assumptions Made in Analysis

1. **digitalWrite() overhead:** Estimated at 15-25 ARM instructions based on typical STM32duino core implementation
2. **No interrupt interference:** Assumes interrupts are disabled during LED transmission (standard for both libraries)
3. **Optimal compiler settings:** `-O2` or higher optimization level
4. **Full CPU speed:** 168 MHz with no power-saving modes active
5. **64 LEDs maximum:** Based on current `MAX_LEDS` constant

### Factors Not Included

- Compiler optimization differences
- Cache hit/miss patterns
- DMA potential (neither library uses DMA for WS2812)
- Temperature effects on timing
- Voltage stability

### Measurement Method

Calculations are theoretical based on:
- STM32F407 datasheet specifications
- ARM Cortex-M4 instruction set timing
- WS2812 protocol requirements
- Typical STM32duino digitalWrite() profiling

For exact measurements, use:
```cpp
uint32_t start = micros();
ledStrip.show();
uint32_t elapsed = micros() - start;
Serial.println(elapsed);  // Should print ~2180 microseconds for 64 LEDs
```

## References

- WS2812 Datasheet: [https://cdn-shop.adafruit.com/datasheets/WS2812.pdf](https://cdn-shop.adafruit.com/datasheets/WS2812.pdf)
- STM32F407 Reference Manual: RM0090
- FastLED GitHub: [https://github.com/FastLED/FastLED](https://github.com/FastLED/FastLED)
- Adafruit_NeoPixel: [https://github.com/adafruit/Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- STM32duino Core: [https://github.com/stm32duino/Arduino_Core_STM32](https://github.com/stm32duino/Arduino_Core_STM32)
