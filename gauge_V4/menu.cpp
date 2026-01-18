/*
 * ========================================
 * MENU SYSTEM FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "menu.h"
#include "globals.h"

void swRead() {
  // Declare debounce variables as static locals to persist between function calls
  // and prevent potential interference from other parts of the code
  static bool stateSW = 1;                      // Current state of encoder switch (1 = not pressed)
  static bool lastStateSW = 1;                  // Previous state of encoder switch
  static unsigned int lastStateChangeTime = 0;  // Timestamp of last switch state change (ms)
  static unsigned int debounceDelay = 50;       // Debounce time in milliseconds
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
  if (result == DIR_CW) {
    // Clockwise rotation - increment menu position
    if (dispArray1[menuLevel] == nMenuLevel) 
      dispArray1[menuLevel] = 0;  // Wrap to beginning
    else 
      dispArray1[menuLevel]++;    // Move to next item
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise rotation - decrement menu position
    if (dispArray1[menuLevel] == 0) 
      dispArray1[menuLevel] = nMenuLevel;  // Wrap to end
    else 
      dispArray1[menuLevel]--;              // Move to previous item
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
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise - decrement offset with wraparound
    if(clockOffset == 0) 
      clockOffset = 23;  // Wrap to 23
    else 
      clockOffset--;     // Decrement
  }
}

void goToLevel0() {
  button = 0;           // Clear button press flag
  dispArray1[0] = 0;    // Set to Settings screen (case 0)
  dispArray1[1] = 0;    // Clear level 1 selection
  dispArray1[2] = 0;    // Clear level 2 selection
  menuLevel = 0;        // Return to top menu level
  nMenuLevel = 14;      // Set to 15 items in main menu (0-indexed)
}
