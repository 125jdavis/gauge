#ifndef GAUGES_H
#define GAUGES_H

// GAUGE SETUP //
#define pwrPin 49           // pin to keep power alive after key is off 
#define speedoMax (100*100)     // maximum mph x100

#define MOTOR_RST 36          // motor driver reset pin

#define M1_SWEEP (58*12)     // range of motion for gauge motor 1 standard X25.168 range 315 degrees at 1/3 degree steps
#define M1_STEP  37         // motor 1 step command
#define M1_DIR   38         // motor 1 direction command

#define M2_SWEEP (58*12)    // range of motion for gauge motor 2
#define M2_STEP  34         // motor 2 step command
#define M2_DIR   35         // motor 2 direction command

#define M3_SWEEP (118*12)    // range of motion for gauge motor 3
#define M3_STEP  33         // motor 3 step command
#define M3_DIR   32         // motor 3 direction command

#define M4_SWEEP (58*12)    // range of motion for gauge motor 4
#define M4_STEP  40         // motor 4 step command
#define M4_DIR   41         // motor 4 direction command

//#define ODO_STEPS 32        // number of steps in one ODO revolution
#define odoSteps 32
#define odoPin1 10
#define odoPin2 11
#define odoPin3 12
#define odoPin4 13          // initialize odometer motor


// LED Tach
// How many leds in your strip?
#define NUM_LEDS 26     // how many warning leds
#define WARN_LEDS 6     // how many warning LEDS on each side of midpoint (shift LEDS included)
#define SHIFT_LEDS 2    // how many shift light LEDS on each side of midpoint
#define TACH_DATA_PIN 22     // which pin sends data to LED tachometer