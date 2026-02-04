/*
 * ========================================
 * GPS FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * STM32 VERSION: Simplified GPS reading without AVR Timer0 interrupt
 */

#include "gps.h"
#include "globals.h"
#include "sensors.h"
#include "outputs.h"

// Create GPS object with Serial3
Adafruit_GPS GPS(&Serial3);

/**
 * fetchGPSdata - Process new GPS data when available
 * 
 * STM32 version: Reads data directly from Serial3 in main loop
 * No interrupt-based reading required
 */
void fetchGPSdata(){
  // Read GPS data from serial port
  char c = GPS.read();
  
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
