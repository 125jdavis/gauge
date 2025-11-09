/*
 * ========================================
 * LED CONTROL FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of LED tachometer control
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "LEDControl.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"

void ledShiftLight(int ledRPM){
  if (ledRPM < tachMin) {
      // black out unused range  
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB::Black;
    }
    return;
  }
  int midPoint = NUM_LEDS/2;
  int blackout_val = map(ledRPM, tachMin, tachMax, midPoint, 0);
 
  //tach normal range 
    for (int i = 0;i <= midPoint - WARN_LEDS; i++){
      leds[i] = CRGB(30, 15 , 0);
    }
    for (int i = midPoint + WARN_LEDS + 1; i < NUM_LEDS; i++){
      leds[i] = CRGB(30, 15 , 0);
    }


  // tach warning range
    for (int i = midPoint - WARN_LEDS - 1;i <= midPoint - SHIFT_LEDS; i++){
      leds[i] = CRGB(80, 10 , 0);
    }
    for (int i = midPoint + SHIFT_LEDS + 1; i <= midPoint + WARN_LEDS; i++){
      leds[i] = CRGB(80, 10 , 0);
    }

  // tach shift light range
    for (int i = midPoint - SHIFT_LEDS - 1;i <= midPoint; i++){
      leds[i] = CRGB(80, 0 , 0);
    }
    for (int i = midPoint; i <= midPoint + SHIFT_LEDS; i++){
      leds[i] = CRGB(80, 0 , 0);
    }
    
  // black out unused range  
    for (int i = midPoint - blackout_val; i <= midPoint + blackout_val-1; i++){
      leds[i] = CRGB::Black;
    }

    // Flash LEDs when shift point is exceeded
    if (RPM > tachMax ){
      if (millis() - timerTachFlash > tachFlashRate){
        
        //Black out the shift LEDs if they are on
        if(tachFlashState == 0){
          for (int i = midPoint - SHIFT_LEDS - 1; i <= midPoint + SHIFT_LEDS; i++){
            leds[i] = CRGB::Black;
          }
        }
        
        timerTachFlash =  millis();             //reset the timer
        tachFlashState = 1 - tachFlashState;    //change the state
      }
    }
  FastLED.show();
}

/*
 * ========================================
 * GPS FUNCTIONS
 * ========================================
 * 
 * Handle GPS data acquisition for speedometer, odometer, and clock
 * Uses Adafruit GPS module with interrupt-based data collection
 */

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