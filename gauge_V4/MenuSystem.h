/*
 * ========================================
 * MENU SYSTEM FUNCTIONS
 * ========================================
 * 
 * This file contains functions for menu navigation and rotary encoder input
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>

/**
 * swRead - Read and debounce rotary encoder switch
 * 
 * Monitors the encoder push button and implements software debouncing.
 */
void swRead();

/**
 * rotate - Rotary encoder interrupt handler
 * 
 * Called by hardware interrupt whenever encoder rotates.
 * Updates menu position based on rotation direction.
 */
void rotate();

/**
 * dispMenu - Multi-level menu system controller for display 1
 * 
 * Implements hierarchical menu navigation using rotary encoder.
 */
void dispMenu();

/**
 * goToLevel0 - Return to top-level menu
 * 
 * Resets menu navigation to level 0.
 */
void goToLevel0();

/**
 * disp2 - Handle display 2 menu selection
 * 
 * Manages the simpler single-level menu for the second display.
 */
void disp2();

/**
 * incrementOffset - Adjust clock time zone offset
 * 
 * Increments or decrements the UTC offset for local time display.
 */
void incrementOffset();

#endif // MENU_SYSTEM_H
