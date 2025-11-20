/*
 * ========================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "utilities.h"
#include "globals.h"
#include "display.h"
#include "outputs.h"
#include <EEPROM.h>

void shutdown (void){
  // Save all display menu positions (4 bytes)
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    EEPROM.update(i, dispArray1[i]);  // Only writes if changed
  }
  
  // Save display 2 selection and units
  EEPROM.update(dispArray2Address, dispArray2[0]);
  EEPROM.update(unitsAddress, units);
  
  // Save odometer values (floats, 4 bytes each)
  EEPROM.put(odoAddress, odo);
  EEPROM.put(odoTripAddress, odoTrip);
  EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);  // Remember fuel level for restart


  // Display shutdown screens
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Return gauge needles to zero position
  motorZeroSynchronous();

  // Wait for gauges to settle
  delay(2000);

  // Double-check that key is still off (in case user turned it back on)
  if (vBatt > 1){
    return;  // Abort shutdown - voltage has returned
  }

  // Cut power to Arduino by releasing power latch
  digitalWrite(PWR_PIN, LOW);  // This will power off the entire system
}
void generateRPM(void){
    static bool rpmSwitch = 0;      // Direction flag for demo RPM sweep - local static
    static int gRPM = 900;          // Generated RPM value for demo mode - local static
    static int analog = 2;          // Test analog value - local static
    static int analogSwitch = 0;    // Direction flag for analog test sweep - local static
    
    // RPM signal generation for demo/testing
    if (rpmSwitch == 0){
      gRPM = gRPM + 120;  // Ramp up by 120 RPM per update
    }
    else if (rpmSwitch == 1) {
      gRPM = gRPM - 160;  // Ramp down by 160 RPM per update
    }
    
    // Reverse direction at limits
    if (gRPM > 7000) rpmSwitch = 1;  // Start ramping down at 7000 RPM
    if (gRPM < 900) rpmSwitch = 0;   // Start ramping up at 900 RPM

    // Optional analog signal generation for testing (currently disabled)
    // if (analogSwitch == 0){
    //   analog = analog + 20; 
    // }
    // else if (analogSwitch == 1) {
    //   analog = analog - 20;
    // }
    // if (analog > 1022) analogSwitch = 1;
    // if (analog < 1) analogSwitch = 0;
}
void serialInputFunc(void){
  // SERIAL INPUT FOR TESTING ONLY
  if (Serial.available() > 0) {
    // Read the incoming data as a string (until newline)
    String inputSer = Serial.readStringUntil('\n');
    
    // Convert the input string to an integer
    int newValue = inputSer.toInt();
    
    // Update the test variable with the new value
    // Uncomment the line for the parameter you want to test:
    //coolantTempCAN = (newValue+273.15)*10 ;  // For temperature testing (convert C to Kelvin*10)
    //fuelLvl = newValue;  // For fuel level testing (gallons or liters)
    
    // Print confirmation of new value
    Serial.println("Updated value of fuel level: " + String(fuelLvl));
    Serial.println("Please enter a new value:");
  }
}
