// Gauge Control Code
// Jesse Davis
// 2-10-2020


// LIBRARIES //

#include <Adafruit_GPS.h>
#include <SwitecX25.h>
#include <TimerOne.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  // modify library for 400KHz communication



// DEFINE //
Adafruit_GPS GPS(&Serial2);   // set serial2 to GPS object
#define GPSECHO  false        // do not send raw GPS data to serial monitor 
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

// FUNCTIONS //
void displayUpdate(); // updates OLED display with most current info
void setupGPS();
long getGPS();        // fetches GPS data from serial buffer?
int gauge1();        // calculates correct position for speedometer gauge motor
int gauge2();        // calculates correct position for input_9 gauge motor
void useInterrupt(boolean v);  // adafruit GPS interrupt

// GAUGE SETUP
// standard X25.168 range 315 degrees at 1/3 degree steps
#define STEPS1 (180*3)  // range of motion for speedometer gauge motor
#define STEPS2 (270*3)  // range of motion for input_8 gauge motor
 

SwitecX25 motor1(STEPS1,34,35,36,37); // initialize motor on pins 34,35,36,37
SwitecX25 motor2(STEPS2,30,31,32,33); // initialize motor on pins 30,31,32,33

// DECLARE GLOBAL VARIABLES //
int t1,t2,testnum,angle1_f;
unsigned long A8_raw, A8_sm, v_g, v_sm, v_100, lag, angle1, angle1_last;
unsigned long v_old=0;
unsigned long v_new=1;
unsigned long t_old=0;
unsigned long t_new=1;
unsigned long timer1 = millis();
unsigned long timer2 = millis();
unsigned long timer3 = millis();
boolean usingInterrupt = false;
float v;


////////  SETUP LOOP  ///////////////////////////////////////////////////////////////

void setup() {
// debugging
Serial.begin(115200);


// I2C OLED display 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (128x32)
 
  display.display();               // display image on buffer (splash screen)
  delay(1000);  

// MOTOR STARTUP
  motor1.currentStep = 315*3; // set degrees of sweep, 3 steps per degree
  motor2.currentStep = 315*3;
  
 // zero out motors by running them against the stops
  motor1.setPosition(0);
  motor2.setPosition(0);
  while (motor1.currentStep > 0 || motor2.currentStep > 0)
  {
      motor1.update();
      motor2.update();
  }
  motor1.currentStep = 0;
  motor2.currentStep = 0;
 
    
 // sweep motors through full range of motion
  motor1.targetStep = STEPS1;
  motor2.targetStep = STEPS2;
  while (motor1.currentStep < STEPS1 || motor2.currentStep < STEPS2)  
  {
      motor1.update();
      motor2.update();
  }

  display.clearDisplay();
  display.setTextSize(1);          // text size
  display.setTextColor(WHITE);     // text color
  display.setCursor(36,8);          // position of line 1
  display.println("WELCOME TO");   // text of line 1
  display.setCursor(34,17);         // position of line 2
  display.println("THE MACHINE");  // text of line 2
  display.display();               // print to display
  display.clearDisplay();          // clear the buffer

//  MOTOR UPDATE TIMER  //;

Timer1.initialize(100);                 // set interrupt period in microseconds
Timer1.attachInterrupt(motorUpdate);    // 

// adafruit GPS setup
  GPS.begin(9600);                                // initialize GPS at 9600 baud
  //GPS.sendCommand(PMTK_SET_BAUD_57600);         // increase baud rate to 57600
  //GPS.begin(57600);                             // initialize GPS at 57600
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);  // set GPS module to fetch NMEA RMC only data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);      // set GPS module to update at 5Hz
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);      // set GPS module position fix to 5Hz

  useInterrupt(true);                             // allow use of interrupts for GPS data fetching

delay(1000);
}


///////  MAIN LOOP /////////////////////////////////////////////////////////////////////////

void loop()
{
      if (millis() - timer3 > 32) {
//          int alpha_a1 = 64;          // exponential moving average alpha value
//          angle1_last = angle1;       // save last gauge angle  
          
            angle1 = gauge1();          // read gauge value and return angle
               Serial.print(millis());
               Serial.print(",");
               Serial.print(v);
               Serial.print(",");
               Serial.print(v_g);
               Serial.print(",");
               Serial.println(angle1);

//          angle1 = (angle1*alpha_a1 + angle1_last*(256-alpha_a1))>>8;  // calculate exponential moving average
          motor1.setPosition(angle1);     // send needle angle to motor       
                        
          int angle2 = gauge2();          // read gauge value and return angle
          motor2.setPosition(angle2);     // send needle angle to motor
        
          void motorUpdate();             // update motor position of all motors
          timer3 = millis();
      }
// DISPLAY  timer loop
      if (millis() - timer1 > 100) {
        timer1 = millis();        // reset timer1       
        dispUpdate();             // update OLED display
        timer1 = millis();
      }

    
// GPS DATA FETCHING LOOP //
    if (GPS.newNMEAreceived()) {
        if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
          return;  // we can fail to parse a sentence in which case we should just wait for another  }
      
      if (millis() - timer2 > 100) { 
        timer2 = millis();        // reset timer2
             unsigned long alpha_0 = 192; // filter coefficeint to set speedometer response rate
               t_old = t_new;                     // save previous time value
               t_new = millis();                  // record time of GPS update
               v_old = v_new;                     // save previous value of velocity                       
               lag = t_new-t_old;                 // time between updated
             v = GPS.speed*1.150779;              // fetch velocity from GPS object, convert to MPH             
             float v2 = GPS.speed*115.0779;       // x100 to preserve hundredth MPH accuracy
             v_100 = (unsigned long)v2;           // convert to unsigned long       
             v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8; //filtered velocity value
                         
       }
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

// OLED DISPLAY UPDATE FUNCTION //
void dispUpdate() {
    display.clearDisplay();             //clear buffer
    display.setTextSize(2);             // text size
    display.setCursor(12,1);
    display.print("SPD:");        // GPS speed
    display.println(v);
    display.setTextSize(1);             // text size
    display.setCursor(12,17);
    //display.print("disp updates:  ");   //counter for debugging
    //display.println(testnum);
    display.print("v_new:  ");   //counter for debugging
    display.println(v_new);
    display.display();                  //print to display
    testnum ++; 
}

//  GAUGE POSITION UPDATE FUNCTION  //
void motorUpdate ()
{
  motor1.update();
  motor2.update();
}


//  GAUGE 1 DATA UPDATE FUNCTION  //
int gauge1 () {

  v_g = map(millis()-lag, t_old, t_new, v_old, v_new);               // interpolate values between GPS data fix
 
  
  if (v_g < 150 || v_g > 14000) {                              // bring speeds below 1.5mph and above 140 mph to zero
    v_g = 0;
  }

  int angle = map( v_g, 0, 6000, 0, STEPS1);                  // calculate angle of gauge 
  return angle;                                               // return angle of motor
  
}

//  GAUGE DATA 2 UPDATE FUNCTION  //
int gauge2 () {
  unsigned long alpha_2 = 4;                                 // smoothing factor (between 0-256)
  A8_raw = analogRead(A8);                                    // read analog values from input_8
  A8_sm = (A8_raw*alpha_2 + A8_sm*(256-alpha_2))>>8;          // calculate smoothed value (EMA filter)
  int  angle = map( A8_sm, 0, 1024, 0, STEPS2);               // calculate angle of motor
  return angle;                                               // return angle of motor
}
