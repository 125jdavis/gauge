/*
 * ========================================
 * LED CONTROL FUNCTIONS
 * ========================================
 * 
 * This file contains functions for LED tachometer control
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>

/**
 * ledShiftLight - Update LED tachometer based on RPM
 * 
 * Controls WS2812 LED strip to display RPM visually.
 * LEDs change color based on RPM:
 * - Green for normal operating range
 * - Yellow/Orange for warning range
 * - Red for shift point
 * - Flashing red when over redline
 * 
 * @param ledRPM - Current engine RPM
 */
void ledShiftLight(int ledRPM);

#endif // LED_CONTROL_H
