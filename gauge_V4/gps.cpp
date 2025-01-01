#include "gps.h"

// FECTCH GPS DATA //
void fetchGPSdata(){
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
    return;  // we can fail to parse a sentence in which case we should just wait for another  }
  
    //if (millis() - timerGPSupdate > GPSupdateRate) { 
      //timerGPSupdate = millis();        // reset timer2
            unsigned long alpha_0 = 256; // filter coefficeint to set speedometer response rate 256 = no filter
              t_old = t_new;                     // save previous time value
              t_new = millis();                  // record time of GPS update
              v_old = v_new;                     // save previous value of velocity                       
              lagGPS = t_new-t_old;                 // time between updated
            v = GPS.speed*1.852;              // fetch velocity from GPS object, convert to km/h from knots            
            float vFloat = GPS.speed*185.2;       // x100 to preserve hundredth km/h accuracy
            v_100 = (unsigned long)vFloat;           // convert to unsigned long,        
            v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8; //filtered velocity value
            // Odometer calculations
            if (v > 2) {  // only integrate speed for ODO if speed exceeds 2 km/h, to avoid false incrementing due to GPS inaccuracy 
              distLast = v * lagGPS * 2.77778e-7;      // km traveled since last GPS message
            } else {
              distLast = 0;
            }
            odo = odo + distLast;
            odoTrip = odoTrip + distLast;
            // fetch GMT for clock
            hour = GPS.hour;
            minute = GPS.minute;          
      //}
  }
}

//  ADAFRUIT GPS INTERRUPT FUNCTION  //
//  I don't really understand this code, but it works, so don't freaking mess with it
//  Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}