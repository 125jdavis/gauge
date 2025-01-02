#include "gauges.h"


//  Speedometer Needle Angle Function  //
int speedometerAngle(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;
  float spd_g_float = map(t_curr, t_old, t_new, v_old, v_new)*0.6213712;   // interpolate values between GPS data fix, convert from km/h x100 to mph x100
  spd_g = (unsigned long)spd_g_float;
  if (spd_g < 50) spd_g = 0;                                  // if speed is below 0.5 mph set to zero
  if (spd_g > speedoMax) spd_g = speedoMax;                   // set max pointer rotation
  
  int angle = map( spd_g, 0, speedoMax, 1, sweep-1);         // calculate angle of gauge 
  Serial.print(millis());
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(angle);
  angle = constrain(angle, 1, sweep-1);
  return angle;                                               // return angle of motor
}

int fuelLvlAngle(int sweep) {
  float fuelLvlPct = (fuelLvl/fuelCapacity)*1000;
  fuelLevelPct_g = (unsigned int)fuelLvlPct;
  int angle = map(fuelLevelPct_g, 100, 1000, 1, sweep-1);
  angle = constrain(angle, 1, sweep-1);
  return angle;
} 

int coolantTempAngle(int sweep) {
  int angle;
  if (coolantTemp < 95){
    angle = map((long)coolantTemp, 60, 98, 1, sweep/2);
  }
  else {
    angle = map((long)coolantTemp, 98, 115, sweep/2, sweep-1);
  }
  angle = constrain(angle, 1, sweep-1);
  return angle;
}

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