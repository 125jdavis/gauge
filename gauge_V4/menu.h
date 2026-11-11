/*
 * ========================================
 * MENU SYSTEM FUNCTIONS
 * ========================================
 * 
 * Rotary encoder input and menu navigation
 */

#ifndef MENU_H
#define MENU_H

#include <Arduino.h>

/**
 * swRead - Read and debounce rotary encoder switch
 * 
 * Monitors the encoder push button and implements software debouncing
 * Sets 'button' flag when a valid press-and-release is detected
 * 
 * Called from: main loop (every cycle before display update)
 */
void swRead();

/**
 * rotate - Rotary encoder interrupt handler
 * 
 * Called by hardware interrupt whenever encoder rotates
 * Updates menu position based on rotation direction
 * 
 * Interrupt context: Keep this function fast and simple
 */
void rotate();

/**
 * incrementOffset - Rotary encoder handler for clock offset adjustment
 * 
 * Special interrupt handler used only when adjusting clock offset
 * Replaces the normal 'rotate' handler temporarily
 * 
 * Interrupt context: Keep function fast and simple
 */
void incrementOffset();

/**
 * goToLevel0 - Reset menu navigation to top level
 * 
 * Returns to the main menu (level 0) and resets all menu position variables.
 * Called when exiting settings or after making a selection.
 */
void goToLevel0();

/**
 * Screen list helpers (display 1 scroll list / display 2 selection list)
 *
 * Lists hold screen IDs 1-17 in the order the user wants them; unselected
 * screens are simply absent.
 */
bool isScreenSelectable(byte id);  // false for out-of-range or retired screen IDs (13, 14)
byte screenListIndexOf(const byte *list, byte count, byte id);  // Position of id, or 255 if absent
bool parseScreenList(const char *text, byte *list, byte &count); // "1,5,3" -> list; false if invalid
void setDefaultScreenLists();   // Every screen enabled in numeric order
void loadScreenLists();         // Load from EEPROM (defaults if unset/corrupt) and sanitize selections
void saveScreenLists();         // Persist both lists to EEPROM
void sanitizeScreenSelections(); // Force current display 1/2 screens to be members of their lists

#endif // MENU_H
