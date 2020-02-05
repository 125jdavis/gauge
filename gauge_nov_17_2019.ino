// Gauge Control Code
// Jesse Davis
// 12-17-2016


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
unsigned long v_lo=0;
unsigned long v_hi=1;
unsigned long t_lo=0;
unsigned long t_hi=1;
uint32_t timer1 = millis();
uint32_t timer2 = millis();
uint32_t timer3 = millis();
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

if (timer3 > millis())  timer3 = millis();
      if (millis() - timer3 > 8) {
          int alpha_a1 = 64;
          angle1_last = angle1;
          
          angle1 = gauge1();          // read gauge value and return angle
          angle1 = (angle1*alpha_a1 + angle1_last*(256-alpha_a1))>>8;
          motor1.setPosition(angle1);     // send needle angle to motor       
          
          Serial.print(0);
          Serial.print(" ");
          Serial.print(540);
          Serial.print(" ");
          Serial.println(angle1);
               
          int angle2 = gauge2();          // read gauge value and return angle
          motor2.setPosition(angle2);     // send needle angle to motor
        
          void motorUpdate();             // update motor position of all motors
      }
// DISPLAY  timer loop
  if (timer1 > millis())  timer1 = millis();
      if (millis() - timer1 > 100) {
        timer1 = millis();        // reset timer1
        
      dispUpdate();             // update OLED display
      }

    
// GPS DATA FETCHING LOOP //
    if (GPS.newNMEAreceived()) {
        if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
          return;  // we can fail to parse a sentence in which case we should just wait for another  }
      // if millis() or timer wraps around, we'll just reset it
      if (timer2 > millis())  timer2 = millis();
      if (millis() - timer2 > 200) { 
        timer2 = millis();        // reset timer2
             unsigned long alpha_0 = 128; // filter coefficeint to set speedometer response rate
          
             v_lo = v_hi;                    // save previous value of velocity    
             t_lo = t_hi;                  // save previous time value
             lag = t_hi-t_lo;
             v = GPS.speed*1.150779;            // fetch velocity from GPS object, convert to MPH             
             t_hi = micros();                 // get current time value
             v_100 = (unsigned long)v*100;               // x100 to preserve hundredth MPH accuracy         
             v_hi = (v_100*alpha_0 + v_hi*(256-alpha_0))>>8; //filtered velocity value
             
            // Serial.println(v_hi);
             
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
    display.print("v_hi:  ");   //counter for debugging
    display.println(v_hi);
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

  v_g = map(micros()-lag, t_lo,t_hi,v_lo,v_hi);               // interpolate values between GPS data fix
  if (v_g < 150 || v_g > 20000) {                              // bring speeds below 1.5mph and above 200 mph to zero
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
