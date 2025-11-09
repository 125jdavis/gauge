/*
 * ========================================
 * GPS FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of GPS processing functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "GPSFunctions.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"

void fetchGPSdata(){
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))   // Parse NMEA sentence; also clears newNMEAreceived flag
    return;  // If parse fails (corrupt data), wait for next sentence
  
    //if (millis() - timerGPSupdate > GPSupdateRate) {  // Optional rate limiting (currently disabled)
      //timerGPSupdate = millis();
      
            unsigned long alpha_0 = 256;  // Filter coefficient (256 = no filtering, instant response)
            
            // Save previous values for interpolation
            t_old = t_new;        // Previous timestamp
            t_new = millis();     // Current timestamp
            v_old = v_new;        // Previous filtered speed
            lagGPS = t_new-t_old; // Time between GPS updates (typically 200ms at 5Hz)
            
            // Get speed from GPS and convert units
            v = GPS.speed*1.852;           // Convert knots to km/h (1 knot = 1.852 km/h)
            float vFloat = GPS.speed*185.2;  // Speed * 100 for precision (km/h * 100)
            v_100 = (unsigned long)vFloat;   // Convert to integer
            
            // Apply exponential filter for smooth speedometer
            v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8;  // Weighted average (>>8 = /256)
            
            // Calculate distance traveled for odometer
            if (v > 2) {  // Only integrate if speed > 2 km/h (reduces GPS drift errors)
              distLast = v * lagGPS * 2.77778e-7;  // Distance (km) = speed (km/h) * time (ms) * conversion factor
            } else {
              distLast = 0;  // Don't increment odometer when stationary
            }
            odo = odo + distLast;        // Update total odometer
            odoTrip = odoTrip + distLast;  // Update trip odometer
            
            // Extract time from GPS (UTC)
            hour = GPS.hour;
            minute = GPS.minute;          
      //}
  }
}

/**
 * TIMER0_COMPA_vect - Timer0 compare interrupt for GPS data reading
 * 
 * This interrupt service routine (ISR) is called automatically once per millisecond
 * by the Arduino Timer0 hardware timer. It reads one byte from the GPS module
 * without blocking the main loop.
 * 
 * The GPS module sends NMEA sentences at 9600 baud (960 characters/second).
 * This ISR ensures no characters are missed even when main loop is busy.
 * 
 * Interrupt context: Keep fast and simple - just read one character
 * 
 * Note: Original comment preserved - this is boilerplate code from Adafruit GPS library
 */
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();  // Read one byte from GPS module
  // Debug option: echo GPS data to serial (very slow - only for debugging)
#ifdef UDR0  // Check if UART data register is defined (AVR specific)
  if (GPSECHO)
    if (c) UDR0 = c;  // Write directly to UART register (faster than Serial.print)
    // Writing direct to UDR0 is much faster than Serial.print 
    // but only one character can be written at a time
#endif
}

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

void useInterrupt(boolean v) {
  if (v) {
    // Enable GPS reading via Timer0 interrupt
    // Timer0 is already used for millis() - we add our interrupt to it
    OCR0A = 0xAF;  // Set compare value (when timer reaches this, interrupt fires)
    TIMSK0 |= _BV(OCIE0A);  // Enable Timer0 Compare A interrupt
    usingInterrupt = true;
  } else {
    // Disable GPS interrupt
    TIMSK0 &= ~_BV(OCIE0A);  // Disable Timer0 Compare A interrupt
    usingInterrupt = false;
  }
}

/*
 * ========================================
 * STEPPER MOTOR ANGLE CALCULATION FUNCTIONS
 * ========================================
 * 
 * Convert sensor values to motor angles for gauge needle positioning
 */

/**
 * speedometerAngle - Calculate speedometer needle angle from GPS speed
 * 
 * Interpolates between GPS updates for smooth needle movement and converts
 * speed to motor steps. Includes clamping and dead zone logic.
 * 
 * @param sweep - Maximum motor steps for full gauge sweep (e.g., 1416 for M3)
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Algorithm:
 * 1. Interpolate speed between GPS updates (5Hz to ~50Hz for smooth motion)
 * 2. Convert km/h to mph (* 0.6213712)
 * 3. Apply dead zone (< 0.5 mph reads as 0)
 * 4. Clamp to maximum (100 mph)
 * 5. Map speed to motor angle (0-100 mph -> 1 to sweep-1 steps)
 * 
 * Note: Serial.print statements are for debugging speed values
 */