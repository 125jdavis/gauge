
#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 32
// how many warning leds
#define WARN_LEDS 6
// how many shift
#define SHIFT_LEDS 2 
#define DATA_PIN 7

// Define the array of leds
CRGB leds[NUM_LEDS];

long unsigned timer = 0;          // used to control RPM signal generator
long unsigned rpmRate = 50;       // how often is the RPM signal generator called
long unsigned timerTachFlash = 0; // used to control shift light flashing
long unsigned tachFlashRate = 50; // how often does the shift light flash
int RPM = 4000;                   // initial RPM for generated signal
bool rpmSwitch = 0;               // state of RPM generator
bool tachFlashState = 0;          // state of shift light flashing
int tachMin = 4000;               // lowest RPM shown on tachometer
int tachMax = 6500;               // highest RPM shown on tach, shift light begins flashing

void setup() { 
  Serial.begin(115200);
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical

}

void loop() { 


  if (millis() - timer > rpmRate){

    // RPM signal generation for demo
    if (rpmSwitch == 0){
      RPM = RPM + 100; 
    }
    else if (rpmSwitch == 1) {
      RPM = RPM - 140;
    }
    if (RPM > 7000) rpmSwitch = 1;
    if (RPM < 3000) rpmSwitch = 0;
    timer = millis();

    ledShiftLight(RPM);
    
  }


}


void ledShiftLight(int ledRPM){
  int blackout_val = map(ledRPM, tachMin, tachMax, NUM_LEDS/2, 0);
  int midPoint = NUM_LEDS;

  //tach normal range 
    for (int i = 0;i <= midPoint - WARN_LEDS; i++){
      leds[i] = CRGB(100, 38 , 0);
    }
    for (int i = midPoint + WARN_LEDS + 1; i < NUM_LEDS; i++){
      leds[i] = CRGB(100, 38 , 0);
    }


  // tach warning range
    for (int i = midPoint - WARN_LEDS - 1;i <= midPoint - SHIFT_LEDS; i++){
      leds[i] = CRGB(255, 30 , 0);
    }
    for (int i = midPoint + SHIFT_LEDS + 1; i <= midPoint + WARN_LEDS; i++){
      leds[i] = CRGB(255, 30 , 0);
    }

  // tach shift light range
    for (int i = midPoint - SHIFT_LEDS - 1;i <= midPoint; i++){
      leds[i] = CRGB(255, 0 , 0);
    }
    for (int i = midPoint; i <= midPoint + SHIFT_LEDS; i++){
      leds[i] = CRGB(255, 0 , 0);
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
        
        timer =  millis();                      //reset the timer
        tachFlashState = 1 - tachFlashState;    //change the state
      }
    }
  FastLED.show();
}
