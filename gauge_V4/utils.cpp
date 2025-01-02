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