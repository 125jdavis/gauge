/*
 * ========================================
 * GPS FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "gps.h"
#include "globals.h"
#include "sensors.h"
#include "outputs.h"

/**
 * fetchGPSdata - Process new GPS data when available
 */
void fetchGPSdata(){
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))   // Parse NMEA sentence; also clears newNMEAreceived flag
    return;  // If parse fails (corrupt data), wait for next sentence
  
    //if (millis() - timerGPSupdate > GPS_UPDATE_RATE) {  // Optional rate limiting (currently disabled)
      //timerGPSupdate = millis();
      
            constexpr unsigned int ALPHA_GPS = 256;  // Filter coefficient (256 = no filtering, instant response)
            
            // Save previous values for interpolation
            t_old = t_new;        // Previous timestamp
            t_new = millis();     // Current timestamp
            v_old = spdGPS;       // Previous filtered speed
            lagGPS = t_new - t_old; // Time between GPS updates (typically 200ms at 5Hz)
            
            // Get speed from GPS and convert units
            v = GPS.speed * 1.852;           // Convert knots to km/h (1 knot = 1.852 km/h)
            float vFloat = GPS.speed * 185.2;  // Speed * 100 for precision (km/h * 100)
            v_100 = (unsigned long)vFloat;   // Convert to integer
            
            // Apply exponential filter for smooth speedometer
            spdGPS = (v_100 * ALPHA_GPS + v_old * (256 - ALPHA_GPS)) >> 8;  // Weighted average (>>8 = /256)
            
            // Calculate distance traveled for odometer (only if GPS is selected as speed source)
            if (SPEED_SOURCE == 3) {
              distLast = updateOdometer(v, lagGPS);
              if (distLast > 0) {
                moveOdometerMotor(distLast);
              }
            } else {
              // Still calculate distLast for potential display/debugging, but don't update odometer
              if (v > 2) {
                distLast = v * lagGPS * 2.77778e-7;
              } else {
                distLast = 0;
              }
            }
            
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
