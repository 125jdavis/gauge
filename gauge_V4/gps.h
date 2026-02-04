/*
 * ========================================
 * GPS FUNCTIONS
 * ========================================
 * 
 * Handle GPS data acquisition for speedometer, odometer, and clock
 * STM32 VERSION: Simplified polling-based GPS reading via UART3
 */

#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <Adafruit_GPS.h>

// GPS object declaration (defined in gps.cpp)
extern Adafruit_GPS GPS;

/**
 * fetchGPSdata - Process new GPS data when available
 * 
 * STM32 version: Reads GPS data directly from Serial3 in main loop
 * No interrupt-based reading required
 * 
 * Parses NMEA sentences from GPS module and updates speed, odometer, and time.
 * Uses filtering and interpolation for smooth speedometer response.
 * 
 * Processing steps:
 * 1. Read GPS character from serial port
 * 2. Check if new NMEA sentence received
 * 3. Parse sentence (fails silently if corrupt)
 * 4. Record timestamps for interpolation
 * 5. Convert speed from knots to km/h
 * 6. Apply exponential filtering for smooth display
 * 7. Calculate distance traveled since last update
 * 8. Update total and trip odometers
 * 9. Extract time (hour, minute) for clock
 * 
 * Global variables modified:
 * - v: Speed in km/h (float)
 * - spdGPS, v_old: Filtered speed values (km/h * 100)
 * - t_new, t_old: Timestamps
 * - lagGPS: Time since last GPS update
 * - odo, odoTrip: Odometer values (if SPEED_SOURCE == 3)
 * - hour, minute: GPS time (UTC)
 * 
 * Called from: main loop every 1ms
 */
void fetchGPSdata();

#endif // GPS_H
