# OLED Display Performance Audit Report

**Date:** 2026-02-05  
**Repository:** 125jdavis/gauge  
**Purpose:** Analyze OLED display code for opportunities to reduce time spent writing to displays

---

## Executive Summary

The OLED display system currently updates **both displays at 10Hz (100ms interval)**. Each display function performs a **full clearDisplay() and display() cycle**, transmitting the entire framebuffer (128x32 pixels = 512 bytes) over SPI for every update, regardless of whether content has changed.

**Key Finding:** Display updates consume significant CPU time due to:
1. Unconditional full-screen clears and redraws every 100ms
2. No dirty region tracking or partial updates
3. No caching of previous display state
4. Static content (logos, labels) redrawn every frame
5. Redundant operations when data hasn't changed

**Potential Time Savings:** 50-90% reduction in display update time through selective updates, dirty tracking, and static content caching.

---

## Current Implementation Analysis

### Display Update Architecture

**Location:** `gauge_V4.ino` lines 383-387
```cpp
if (millis() - timerDispUpdate > DISP_UPDATE_RATE) {
    dispMenu();   // Update display 1 (menu/data)
    disp2();      // Update display 2 (selected data)
    timerDispUpdate = millis();
}
```

**Update Rate:** `DISP_UPDATE_RATE = 100ms` (10Hz) - `config_hardware.h` line 113

### Display Function Pattern

**Every display function follows this pattern:**
```cpp
void dispOilPrsGfx(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();           // Clear entire framebuffer
    display->drawBitmap(...);          // Draw icon
    display->setTextSize(3);
    display->setCursor(...);
    display->print(value);             // Draw text
    display->display();                // Transmit entire framebuffer to OLED
}
```

**Operations per update:**
1. `clearDisplay()` - Zeros 512-byte framebuffer (128x32 bits)
2. Multiple drawing operations (text, bitmaps, shapes)
3. `display()` - SPI transmission of entire 512-byte buffer

### Performance Impact

**SPI Transmission Time (approximate):**
- SPI clock on SSD1306: typically 8 MHz
- 512 bytes × 8 bits = 4096 bits
- Transmission time: 4096 bits ÷ 8 MHz = **~512 μs per display**
- **Two displays = ~1024 μs (1ms) in SPI alone**

**Total Display Update Time:**
- SPI transmission: ~1ms
- Framebuffer operations: ~200-500μs
- **Total: ~1.2-1.5ms per 100ms cycle**
- **CPU overhead: ~1.2-1.5%**

While not currently a bottleneck, this can be significantly reduced.

---

## Performance Optimization Opportunities

### 1. **Dirty Region Tracking** (HIGH IMPACT)

**Problem:** Every frame, both displays are completely cleared and redrawn, even when content hasn't changed.

**Solution:** Track when data values change and only update when necessary.

**Implementation Example:**
```cpp
// In globals.h - Add previous value tracking
extern int RPM_prev;
extern float oilPrs_prev;
extern float battVolt_prev;
// ... for each displayed value

// In display.cpp - Add dirty checking
void dispRPM(Adafruit_SSD1306 *display) {
    // Only update if RPM changed significantly (reduce noise)
    if (abs(RPM - RPM_prev) < 10) {
        return;  // Skip update, display unchanged
    }
    RPM_prev = RPM;
    
    // Existing drawing code
    display->clearDisplay();
    // ... draw content ...
    display->display();
}
```

**Expected Savings:**
- RPM: Updates ~20-30x per second when changing, 0x when steady
- Oil pressure: Rarely changes, could skip 90%+ of updates
- Battery voltage: Very stable, could skip 95%+ of updates
- **Overall: 50-70% reduction in display updates**

---

### 2. **Partial Display Updates** (HIGH IMPACT)

**Problem:** `display()` always transmits the entire 512-byte framebuffer, even when only a small number changed.

**Solution:** Use SSD1306's partial update capability to only send changed regions.

**Implementation Example:**
```cpp
// Option A: Use Adafruit_SSD1306::display(x, y, w, h) if available
// Check library version for partial update support

// Option B: Manual partial update (more complex)
void dispRPMPartial(Adafruit_SSD1306 *display) {
    if (abs(RPM - RPM_prev) < 10) return;
    
    // Calculate bounding box for RPM value only
    byte nDig = digits(RPM);
    byte center = 47;
    byte x = center - ((nDig*18)/2);
    byte y = 6;
    byte w = nDig * 18;  // Width in pixels
    byte h = 24;          // Height at size 3
    
    // Clear only the number region (not the whole display)
    display->fillRect(x, y, w, h, BLACK);
    
    // Redraw only the number
    display->setCursor(x, y);
    display->println(RPM);
    
    // Update only the changed region (requires custom implementation)
    // This would need modification to Adafruit_SSD1306 or direct SSD1306 commands
    display->display();  // For now, still full update
    
    RPM_prev = RPM;
}
```

**Expected Savings:**
- Typical value change: Only update 20-50 bytes instead of 512 bytes
- **60-90% reduction in SPI transmission time**

**Note:** This requires either:
1. Adafruit_SSD1306 library support for partial updates (check version)
2. Custom implementation using direct SSD1306 commands
3. Using fillRect to clear only changed regions (simpler, less savings)

---

### 3. **Static Content Caching** (MEDIUM IMPACT)

**Problem:** Static elements (icons, labels) are redrawn every frame.

**Solution:** Draw static content once, only redraw dynamic values.

**Implementation Example:**
```cpp
// Add state tracking
static bool oilPrsIconDrawn = false;

void dispOilPrsGfx(Adafruit_SSD1306 *display) {
    // First time or mode change: draw everything
    if (!oilPrsIconDrawn) {
        display->clearDisplay();
        display->drawBitmap(0, 0, IMG_OIL_PRS, 40, 32, 1);  // Icon drawn once
        oilPrsIconDrawn = true;
    }
    
    // Only update changed value
    if (abs(oilPrs - oilPrs_prev) < 1.0) return;  // Skip if unchanged
    
    // Clear only the value region
    display->fillRect(40, 0, 88, 32, BLACK);  // Clear value area only
    
    // Redraw value
    float oilPrsDisp = (units == 0) ? oilPrs/100 : oilPrs * 0.1450377;
    byte nDig = (units == 0) ? 3 : digits(oilPrsDisp);
    byte center = (units == 0) ? 79 : 71;
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2), 6);
    display->print(oilPrsDisp, (units == 0) ? 1 : 0);
    
    // Label
    if (units == 0) {
        display->setCursor(center+((nDig*18)/2)+3, 18);
        display->setTextSize(1);
        display->println("bar");
    } else {
        display->setCursor(center+((nDig*18)/2)+2, 10);
        display->setTextSize(2);
        display->println("PSI");
    }
    
    display->display();
    oilPrs_prev = oilPrs;
}

// Reset cache when switching displays
void resetDisplayCache() {
    oilPrsIconDrawn = false;
    // ... reset other cached flags
}
```

**Expected Savings:**
- Avoids redrawing ~1600 pixels (icon) every frame
- **10-30% reduction in framebuffer operations**

---

### 4. **Logo Display Optimization** (HIGH IMPACT for static screens)

**Problem:** Static logos (302CID, Falcon Script) are redrawn every 100ms despite never changing.

**Solution:** Skip display() call entirely for static content after initial draw.

**Implementation Example:**
```cpp
// Track if static content has been drawn
static bool falconLogoDrawn = false;
static byte lastDispArray2 = 0xFF;  // Track mode changes

void disp2() {
    // Detect mode change
    if (dispArray2[0] != lastDispArray2) {
        lastDispArray2 = dispArray2[0];
        // Reset all "drawn" flags when mode changes
        falconLogoDrawn = false;
    }
    
    switch (dispArray2[0]) {
        case 7: // 302CID Logo
            if (!falconLogoDrawn) {
                disp302CID(&display2);
                falconLogoDrawn = true;
            }
            // Skip update - logo already displayed
            break;
            
        case 4: // RPM (dynamic)
            dispRPM(&display2);  // Always update dynamic content
            break;
            
        // ... other cases
    }
}
```

**Expected Savings:**
- When displaying static logos: **100% reduction** (0 updates vs 10 per second)
- Affects ~30% of display modes (logos, static text)

---

### 5. **Conditional Update Rate Adjustment** (MEDIUM IMPACT)

**Problem:** 10Hz update rate is constant, even when values change slowly.

**Solution:** Adjust update rate based on content type and rate of change.

**Implementation Example:**
```cpp
// In config_hardware.h - Add adaptive rates
constexpr unsigned int DISP_UPDATE_RATE_FAST = 50;   // 20Hz for fast-changing (RPM, speed)
constexpr unsigned int DISP_UPDATE_RATE_NORMAL = 100; // 10Hz for normal (temps, pressures)
constexpr unsigned int DISP_UPDATE_RATE_SLOW = 500;   // 2Hz for slow (fuel level, battery)

// In gauge_V4.ino - Use adaptive rate
unsigned int currentDispUpdateRate = DISP_UPDATE_RATE_NORMAL;

void loop() {
    // Determine appropriate update rate based on current display mode
    switch (dispArray2[0]) {
        case 4:  // RPM
        case 5:  // Speed
            currentDispUpdateRate = DISP_UPDATE_RATE_FAST;
            break;
        case 3:  // Fuel level
        case 2:  // Battery voltage
            currentDispUpdateRate = DISP_UPDATE_RATE_SLOW;
            break;
        default:
            currentDispUpdateRate = DISP_UPDATE_RATE_NORMAL;
    }
    
    if (millis() - timerDispUpdate > currentDispUpdateRate) {
        dispMenu();
        disp2();
        timerDispUpdate = millis();
    }
}
```

**Expected Savings:**
- Slow-changing displays: **80% reduction** (2Hz vs 10Hz)
- Fast-changing displays: Improved responsiveness with 2x rate

---

### 6. **Minimize Redundant Operations** (LOW-MEDIUM IMPACT)

**Problem:** Multiple redundant operations in each display function.

**Solution:** Consolidate operations, avoid repeated setTextColor(WHITE) calls.

**Implementation Example:**
```cpp
// BEFORE: Redundant operations
void dispRPM(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE);  // Called every time
    display->clearDisplay();
    display->setTextSize(3);
    display->setCursor(...);
    display->println(RPM);
    display->setTextSize(2);        // Change size
    display->setCursor(...);
    display->println("RPM");
    display->display();
}

// AFTER: Optimized
void dispRPM(Adafruit_SSD1306 *display) {
    if (abs(RPM - RPM_prev) < 10) return;  // Skip if unchanged
    
    // Display is already WHITE by default, skip setTextColor
    display->clearDisplay();
    
    // Draw everything at once, minimize state changes
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2), 6);
    display->println(RPM);
    
    display->setTextSize(2);  // Size change only when needed
    display->setCursor(88, 10);
    display->println("RPM");
    
    display->display();
    RPM_prev = RPM;
}
```

**Expected Savings:**
- Reduces function call overhead
- **5-10% reduction in framebuffer operation time**

---

## Recommended Implementation Priority

### Phase 1: Quick Wins (Immediate - Low Risk)
1. **Dirty Region Tracking** - Add previous value variables, skip updates when unchanged
2. **Logo Optimization** - Skip updates for static content
3. **Minimize Redundant Operations** - Code cleanup

**Estimated effort:** 2-4 hours  
**Expected savings:** 40-60% reduction in display updates  
**Risk:** Very low - non-breaking changes

### Phase 2: Adaptive Updates (Short-term - Low Risk)
4. **Conditional Update Rate** - Adjust rate based on content type

**Estimated effort:** 1-2 hours  
**Expected savings:** Additional 10-20% reduction  
**Risk:** Low - backward compatible

### Phase 3: Advanced Optimizations (Medium-term - Medium Risk)
5. **Static Content Caching** - Draw icons/labels once
6. **Partial Display Updates** - Require library investigation/modification

**Estimated effort:** 4-8 hours  
**Expected savings:** Additional 20-40% reduction  
**Risk:** Medium - requires testing, may need library modifications

---

## Code Examples: Complete Refactored Functions

### Example 1: Optimized RPM Display with Dirty Tracking

```cpp
// In globals.h - Add tracking variable
extern int RPM_prev;

// In globals.cpp - Initialize
int RPM_prev = 0;

// In display.cpp - Optimized function
void dispRPM(Adafruit_SSD1306 *display) {
    // Skip update if value hasn't changed significantly
    if (abs(RPM - RPM_prev) < 10) {
        return;  // No update needed
    }
    
    byte nDig = digits(RPM);
    byte center = 47;
    
    display->clearDisplay();
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2), 6);
    display->println(RPM);
    
    display->setTextSize(2);
    display->setCursor(88, 10);
    display->println("RPM");
    
    display->display();
    
    // Update previous value
    RPM_prev = RPM;
}
```

### Example 2: Optimized Static Logo Display

```cpp
// In display.cpp - Add tracking variables
static bool logoDrawn = false;
static byte lastDisplayMode = 0xFF;

void disp2() {
    // Detect mode change
    if (dispArray2[0] != lastDisplayMode) {
        lastDisplayMode = dispArray2[0];
        logoDrawn = false;  // Reset on mode change
    }
    
    switch (dispArray2[0]) {
        case 0: // Oil Pressure (dynamic - always update)
            dispOilPrsGfx(&display2);
            break;
            
        case 7: // 302CID Logo (static - update once)
            if (!logoDrawn) {
                disp302CID(&display2);
                logoDrawn = true;
            }
            // Skip subsequent updates - logo already displayed
            break;
            
        case 8: // 302V Logo (static - update once)
            if (!logoDrawn) {
                disp302V(&display2);
                logoDrawn = true;
            }
            break;
            
        case 9: // Falcon Script (static - update once)
            if (!logoDrawn) {
                dispFalconScript(&display2);
                logoDrawn = true;
            }
            break;
            
        // ... other cases
    }
}
```

### Example 3: Optimized Oil Pressure with Icon Caching

```cpp
// In display.cpp - Add caching state
static bool oilPrsIconCached = false;
static byte lastDispMode = 0xFF;
static float oilPrs_prev = 0.0;

void dispOilPrsGfx(Adafruit_SSD1306 *display) {
    // Detect display mode change (need full redraw)
    if (dispArray1[0] != lastDispMode || dispArray2[0] != lastDispMode) {
        oilPrsIconCached = false;
        lastDispMode = dispArray1[0];  // Track for display 1
    }
    
    // First draw or mode change - draw everything
    if (!oilPrsIconCached) {
        display->clearDisplay();
        display->drawBitmap(0, 0, IMG_OIL_PRS, 40, 32, 1);
        oilPrsIconCached = true;
        oilPrs_prev = -999;  // Force value redraw
    }
    
    // Check if value changed significantly (0.1 bar / 1 PSI threshold)
    float oilPrsDisp = (units == 0) ? oilPrs/100 : oilPrs * 0.1450377;
    float threshold = (units == 0) ? 0.1 : 1.0;
    
    if (abs(oilPrsDisp - oilPrs_prev) < threshold) {
        return;  // Value unchanged, skip update
    }
    
    // Clear only the value region (right side of display)
    display->fillRect(40, 0, 88, 32, BLACK);
    
    // Redraw only the value and unit label
    byte nDig = (units == 0) ? 3 : digits(oilPrsDisp);
    byte center = (units == 0) ? 79 : 71;
    
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2), 6);
    display->print(oilPrsDisp, (units == 0) ? 1 : 0);
    
    if (units == 0) {
        display->setCursor(center+((nDig*18)/2)+3, 18);
        display->setTextSize(1);
        display->println("bar");
    } else {
        display->setCursor(center+((nDig*18)/2)+2, 10);
        display->setTextSize(2);
        display->println("PSI");
    }
    
    display->display();
    oilPrs_prev = oilPrsDisp;
}
```

---

## Performance Impact Summary

### Current Performance
- **Update Frequency:** 10Hz (both displays)
- **CPU Time per Update:** ~1.2-1.5ms
- **CPU Overhead:** ~1.2-1.5%
- **Updates per Second:** 20 (10 per display)
- **Bytes Transmitted per Second:** 10,240 bytes (512 bytes × 2 displays × 10Hz)

### After Phase 1 Optimizations (Quick Wins)
- **Effective Update Frequency:** 2-4Hz average (40-60% reduction)
- **CPU Overhead:** ~0.5-0.7% (**50-60% savings**)
- **Updates per Second:** 4-8
- **Bytes Transmitted per Second:** 2,048-4,096 bytes (**60-80% reduction**)

### After All Optimizations
- **Effective Update Frequency:** 1-2Hz average (80-90% reduction)
- **CPU Overhead:** ~0.2-0.3% (**80-85% savings**)
- **Updates per Second:** 2-4
- **Bytes Transmitted per Second:** 1,024-2,048 bytes (**80-90% reduction**)

---

## Testing Recommendations

After implementing optimizations:

1. **Visual Testing**
   - Verify all display modes render correctly
   - Check for artifacts or incomplete updates
   - Test mode transitions

2. **Performance Testing**
   - Measure actual CPU time with millis() timestamps
   - Monitor update frequency with serial debug output
   - Verify no display lag during rapid value changes

3. **Edge Case Testing**
   - Test with rapidly changing values (RPM sweep)
   - Test with static values (idle engine)
   - Test unit switching (metric/imperial)
   - Test all display mode selections

4. **Regression Testing**
   - Verify existing functionality unchanged
   - Test menu navigation
   - Test settings changes

---

## Additional Considerations

### Memory Impact
- Adding `_prev` variables: ~40 bytes (minimal)
- Adding cache flags: ~10 bytes (minimal)
- **Total additional RAM:** ~50 bytes (<1% of Arduino Mega's 8KB)

### Code Complexity
- Phase 1: Low - simple conditional checks
- Phase 2: Low - configuration changes
- Phase 3: Medium - state management required

### Maintenance
- Clear documentation of caching behavior needed
- Consider adding debug mode to force full redraws

### Future Enhancements
- Consider DMA for SPI transfers (hardware dependent)
- Explore buffering multiple partial updates
- Add display sleep/wake for power savings

---

## Conclusion

The current OLED display system works correctly but has significant optimization opportunities. The recommended phased approach provides:

1. **Immediate benefits** with low risk (Phase 1)
2. **Incremental improvements** over time (Phases 2-3)
3. **50-90% reduction** in display update overhead
4. **Minimal code complexity** increase

**Primary recommendations:**
1. Implement dirty region tracking (Phase 1) - **Highest ROI**
2. Optimize static content (Phase 1) - **Easy win**
3. Consider partial updates (Phase 3) - **Advanced optimization**

No code changes should be committed at this time per user request. This report provides the roadmap for future implementation when authorized.

---

**Report prepared by:** GitHub Copilot Agent  
**Status:** Analysis complete, ready for implementation approval
