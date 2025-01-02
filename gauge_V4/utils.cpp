#include "utils.h"

// Signal selection  //
void sigSelect (void) {
    spd = v_new; //km/h * 100
    //spdMph = spd *0.6213712;
    spdCAN = (int)(v*16);
    RPM = rpmCAN;
    coolantTemp = (coolantTempCAN/10)-273.15; // convert kelvin to C;
    oilPrs = (oilPrsCAN/10)-101.3;   //kPa, convert to gauge pressure
    fuelPrs = (fuelPrsCAN/10)-101.3;  //kPa, convert to gauge pressure
    oilTemp = therm;
    afr = (float)afr1CAN/1000;
    fuelComp = fuelCompCAN/10;
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);

}

void shutdown (void){
  // Write dispArray1 values from into EEPROM address 0-3
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    EEPROM.update(i, dispArray1[i]);
  }
  
  // Write dispArray2 values from into EEPROM for disp array 2
  EEPROM.update(dispArray2Address, dispArray2[0]);
  EEPROM.update(unitsAddress, units);
  EEPROM.put(odoAddress, odo);
  EEPROM.put(odoTripAddress, odoTrip);
  EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);


  // Display 
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Zero the motors
  motorZeroSynchronous();

  // delay
  delay(2000);

  // check again for key off
  if (vBatt > 1){
    return;
  }

  // cut power to Dash control unit
    digitalWrite(pwrPin, LOW);
}