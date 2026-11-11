/*
 * ========================================
 * MENU SYSTEM FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "menu.h"
#include "globals.h"
#include <EEPROM.h>
#include <string.h>

void swRead() {
  // Declare debounce variables as static locals to persist between function calls
  // and prevent potential interference from other parts of the code
  static bool stateSW = 1;                      // Current state of encoder switch (1 = not pressed)
  static bool lastStateSW = 1;                  // Previous state of encoder switch
  static unsigned long lastStateChangeTime = 0;  // Timestamp of last switch state change (ms)
  static unsigned int debounceDelay = 50;       // Debounce time in milliseconds (50ms never overflows)
  static bool debounceFlag = 0;                 // Flag to prevent multiple triggers during debounce
       
  stateSW = digitalRead(SWITCH);            // Read current state of encoder button
  int stateChange = stateSW - lastStateSW;  // Calculate change: -1=pressed, +1=released, 0=no change

  // Clear debounce flag if enough time has passed since last change
  if ((millis() - lastStateChangeTime) > debounceDelay) {
    debounceFlag = 0;  // Allow new state changes to be registered
  }

  // Detect button press (falling edge) - record time but don't trigger action yet
  if (stateChange < 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of press
    debounceFlag = 1;                // Block bounces
  } 
  // Detect button release (rising edge) - this is when we register the button press
  else if (stateChange > 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of release
    debounceFlag = 1;                // Block bounces
    button = 1;                      // Set button event flag (cleared by menu handlers)
  } 
  else if (stateChange == 0) {  
    // No state change - do nothing
  }
  lastStateSW = stateSW;  // Save current state for next comparison
}

/**
 * rotate - Rotary encoder interrupt handler for menu navigation
 * 
 * ISR Design: Lightweight and deterministic
 * - Called on CHANGE for encoder pins (2, 3)
 * - Processes quadrature encoder signals via Rotary library
 * - Updates menu position variable (dispArray1[menuLevel])
 * - No blocking operations, no SPI, no Serial
 * 
 * Performance: ~5-10 µs execution time
 * 
 * Note: This ISR modifies shared variables (dispArray1) that are also
 * accessed by main loop. AVR byte writes are atomic, so no race condition.
 */
void rotate() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  if (result != DIR_CW && result != DIR_CCW) return;

  if (menuLevel == 0) {
    // Top level: step through the user's screen list; Settings (ID 0) is always the last stop
    byte pos = disp1Count;  // Settings slot (also used when current screen is not in the list)
    for (byte i = 0; i < disp1Count; i++) {
      if (disp1List[i] == dispArray1[0]) { pos = i; break; }
    }
    if (result == DIR_CW) {
      pos = (pos >= disp1Count) ? 0 : pos + 1;
    } else {
      pos = (pos == 0) ? disp1Count : pos - 1;
    }
    dispArray1[0] = (pos >= disp1Count) ? 0 : disp1List[pos];
    encoderMoved = true;
    return;
  }

  if (result == DIR_CW) {
    // Clockwise rotation - increment menu position
    if (dispArray1[menuLevel] == nMenuLevel) 
      dispArray1[menuLevel] = 0;  // Wrap to beginning
    else 
      dispArray1[menuLevel]++;    // Move to next item
    encoderMoved = true;  // Set flag for immediate display update
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise rotation - decrement menu position
    if (dispArray1[menuLevel] == 0) 
      dispArray1[menuLevel] = nMenuLevel;  // Wrap to end
    else 
      dispArray1[menuLevel]--;              // Move to previous item
    encoderMoved = true;  // Set flag for immediate display update
  }
}

/**
 * incrementOffset - Rotary encoder interrupt handler for clock offset adjustment
 * 
 * ISR Design: Lightweight and deterministic
 * - Called on CHANGE for encoder pins during clock offset adjustment
 * - Processes quadrature encoder signals via Rotary library
 * - Updates clockOffset variable (0-23 hours)
 * - No blocking operations, no SPI, no Serial
 * 
 * Performance: ~5-10 µs execution time
 * 
 * Note: This ISR is temporarily attached during clock offset adjustment,
 * replacing the normal rotate() ISR. See display.cpp for context.
 */
void incrementOffset() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  
  if (result == DIR_CW) {
    // Clockwise - increment offset with wraparound
    if (clockOffset == 23) 
      clockOffset = 0;  // Wrap to 0
    else 
      clockOffset++;    // Increment
    encoderMoved = true;  // Set flag for immediate display update
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise - decrement offset with wraparound
    if(clockOffset == 0) 
      clockOffset = 23;  // Wrap to 23
    else 
      clockOffset--;     // Decrement
    encoderMoved = true;  // Set flag for immediate display update
  }
}

void goToLevel0() {
  button = 0;           // Clear button press flag
  dispArray1[0] = 0;    // Set to Settings screen (case 0)
  dispArray1[1] = 0;    // Clear level 1 selection
  dispArray1[2] = 0;    // Clear level 2 selection
  menuLevel = 0;        // Return to top menu level
  nMenuLevel = 17;      // Maximum index is 17 (18 total items: indices 0-17)
}

// Screen IDs 13 (Injector Duty) and 14 (Ignition Timing) are retired. The
// numbering of the other screens is kept so saved lists and config files still
// load; retired IDs are silently dropped from any list.
bool isScreenSelectable(byte id) {
  return id >= 1 && id <= MAX_SCREEN_LIST && id != 13 && id != 14;
}

byte screenListIndexOf(const byte *list, byte count, byte id) {
  for (byte i = 0; i < count; i++) {
    if (list[i] == id) return i;
  }
  return 255;
}

// Accepts "id,id,..." with IDs 1-17, no duplicates, at least one selectable entry.
// Retired IDs (13, 14) are accepted but dropped.
bool parseScreenList(const char *text, byte *list, byte &count) {
  byte tmp[MAX_SCREEN_LIST];
  byte n = 0;
  const char *p = text;
  while (*p) {
    if (*p < '0' || *p > '9') return false;
    int v = 0;
    while (*p >= '0' && *p <= '9') {
      v = v * 10 + (*p - '0');
      if (v > 99) return false;
      p++;
    }
    if (v < 1 || v > MAX_SCREEN_LIST) return false;
    if (n >= MAX_SCREEN_LIST || screenListIndexOf(tmp, n, (byte)v) != 255) return false;
    if (isScreenSelectable((byte)v)) tmp[n++] = (byte)v;
    if (*p == ',') {
      p++;
      if (*p == '\0') return false;
    } else if (*p != '\0') {
      return false;
    }
  }
  if (n == 0) return false;
  noInterrupts();  // rotate() ISR reads these lists
  memcpy(list, tmp, n);
  count = n;
  interrupts();
  return true;
}

void setDefaultScreenLists() {
  noInterrupts();
  byte n = 0;
  for (byte id = 1; id <= MAX_SCREEN_LIST; id++) {
    if (!isScreenSelectable(id)) continue;
    disp1List[n] = id;
    disp2List[n] = id;
    n++;
  }
  disp1Count = n;
  disp2Count = n;
  interrupts();
}

static bool loadOneScreenList(uint16_t address, byte *list, byte &count) {
  byte n = EEPROM.read(address);
  if (n < 1 || n > MAX_SCREEN_LIST) return false;
  byte tmp[MAX_SCREEN_LIST];
  byte kept = 0;
  for (byte i = 0; i < n; i++) {
    byte id = EEPROM.read(address + 1 + i);
    if (id < 1 || id > MAX_SCREEN_LIST || screenListIndexOf(tmp, kept, id) != 255) return false;
    if (isScreenSelectable(id)) tmp[kept++] = id;  // Drop retired screens from older saved lists
  }
  if (kept == 0) return false;
  noInterrupts();
  memcpy(list, tmp, kept);
  count = kept;
  interrupts();
  return true;
}

void loadScreenLists() {
  setDefaultScreenLists();
  loadOneScreenList(disp1ListAddress, disp1List, disp1Count);
  loadOneScreenList(disp2ListAddress, disp2List, disp2Count);
  sanitizeScreenSelections();
}

void saveScreenLists() {
  EEPROM.update(disp1ListAddress, disp1Count);
  for (byte i = 0; i < disp1Count; i++) EEPROM.update(disp1ListAddress + 1 + i, disp1List[i]);
  EEPROM.update(disp2ListAddress, disp2Count);
  for (byte i = 0; i < disp2Count; i++) EEPROM.update(disp2ListAddress + 1 + i, disp2List[i]);
}

void sanitizeScreenSelections() {
  if (dispArray1[0] != 0 && screenListIndexOf(disp1List, disp1Count, dispArray1[0]) == 255) {
    dispArray1[0] = disp1List[0];
  }
  if (screenListIndexOf(disp2List, disp2Count, dispArray2[0]) == 255) {
    dispArray2[0] = disp2List[0];
  }
}
