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

#endif // MENU_H
