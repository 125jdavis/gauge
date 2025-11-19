/*
 * ========================================
 * GPS FUNCTIONS
 * ========================================
 * 
 * Handle GPS data acquisition for speedometer, odometer, and clock
 * Uses Adafruit GPS module with interrupt-based data collection
 */

#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

/**
 * fetchGPSdata - Process new GPS data when available
 * 
 * Parses NMEA sentences from GPS module and updates speed, odometer, and time.
 * Uses filtering and interpolation for smooth speedometer response.
 * 
 * Processing steps:
 * 1. Check if new NMEA sentence received
 * 2. Parse sentence (fails silently if corrupt)
 * 3. Record timestamps for interpolation
 * 4. Convert speed from knots to km/h
 * 5. Apply exponential filtering for smooth display
 * 6. Calculate distance traveled since last update
 * 7. Update total and trip odometers
 * 8. Extract time (hour, minute) for clock
 * 
 * Global variables modified:
 * - v: Speed in km/h (float)
 * - v_new, v_old: Filtered speed values
 * - t_new, t_old: Timestamps
 * - lagGPS: Time since last GPS update
 * - odo, odoTrip: Odometer values
 * - hour, minute: GPS time (UTC)
 * 
 * Called from: main loop every 1ms
 * 
 * Note: Distance calculation uses formula: distance = speed * time * (1 km/h = 2.77778e-7 km/ms)
 */
void fetchGPSdata();

/**
 * useInterrupt - Enable or disable GPS interrupt-based reading
 * 
 * Configures Timer0 to trigger GPS character reading via interrupt.
 * Timer0 is normally used for millis() function - this adds an additional
 * interrupt handler that piggybacks on the existing timer.
 * 
 * @param v - true to enable interrupts, false to disable
 * 
 * Hardware configuration:
 * - Uses Timer0 Output Compare A (OCR0A)
 * - Triggers at count 0xAF (175 in decimal)
 * - Interrupt fires ~1000 times per second
 * 
 * Global variables modified:
 * - usingInterrupt: Flag tracking interrupt state
 */
void useInterrupt(boolean v);

#endif // GPS_H
