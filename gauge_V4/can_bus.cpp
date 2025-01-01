#include "can_bus.h"

void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4) //Send input value to CAN BUS
{
        // LITTLE  ENDIAN
        // Word 1 
        data[0] = lowByte(inputVal_1);
        data[1] = highByte(inputVal_1);
        // Word 2 
        data[2] = lowByte(inputVal_2);
        data[3] = highByte(inputVal_2);
        // Word 3
        data[4] = lowByte(inputVal_3);
        data[5] = highByte(inputVal_3);
        // Word 4 
        data[6] = lowByte(inputVal_4);
        data[7] = highByte(inputVal_4);

        //Serial.println(inputVal_1);
        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);
}

void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4) //Send input value to CAN BUS
{
        // BIG ENDIAN
        // Word 1
        data[0] = highByte(inputVal_1);
        data[1] = lowByte(inputVal_1);
        // Word 2 
        data[2] = highByte(inputVal_2);
        data[3] = lowByte(inputVal_2);
        // Word 3 
        data[4] = highByte(inputVal_3);
        data[5] = lowByte(inputVal_3);
        // Word 4
        data[6] = highByte(inputVal_4);
        data[7] = lowByte(inputVal_4);

        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);
}

void receiveCAN ()  //Recive message from CAN BUS
{
  
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    for (byte i =0; i< len; i++){
      canMessageData[i] = rxBuf[i];
      //Serial.println(canMessageData[i]);
    }
//    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
//      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
//    else
//      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
//  
//    Serial.print(msgString);
//  
//    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
//      sprintf(msgString, " REMOTE REQUEST FRAME");
//      Serial.print(msgString);
//    } else {
//      for(byte i = 0; i<len; i++){
//        sprintf(msgString, " 0x%.2X", rxBuf[i]);
//        Serial.print(msgString);
//      }
////      // report value of sensor sent across CAN Bus in human readable format
////        float var = (rxBuf[0]<<8) + rxBuf[1];
////        Serial.print("Volts:");
////        Serial.println(var/100);
//    }      
//    Serial.println();
}


void parseCAN( unsigned long id, unsigned long msg)
{
  int var1 = 0;
  
  if (id == 0x301) {  //test 
    pumpPressureCAN = (rxBuf[0]<<8) + rxBuf[1];
  }
  else if (id == 0x360){  //Haltech Protocol
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];
    //Serial.print("RPM:");
    //Serial.println(rpmCAN);   // y = x

    mapCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("MAP:");
    // Serial.println(mapCAN/10);   // y = x/10

    tpsCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("TPS:");
    // Serial.println(tpsCAN/10);   // y = x/10
  }
  else if (id == 0x361){  //Haltech Protocol
    fuelPrsCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("FuelPressure:");
    // Serial.println(fuelPrsCAN/10 - 101.3);   // y = x/10 - 101.3

    oilPrsCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("Oil Pressure:");   // y = x/10 - 101.3
    // Serial.println(oilPrsCAN/10 - 101.3); 
  }
  else if (id == 0x362){  //Haltech Protocol
    injDutyCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Injector DC:");
    // Serial.println(injDutyCAN/10);   // y = x/10

    ignAngCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Ignition Angle:");   // y = x/10
    // Serial.println(ignAngCAN/10); 
  }
  else if (id == 0x368){  //Haltech Protocol
    afr1CAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("AFR:");
    // Serial.println(afr1CAN/1000);   // y = x/1000
  }
  else if (id == 0x368){  //Haltech Protocol
    knockCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Knock Level:");
    // Serial.println(knockCAN/100);   // y = x/100
  }
  else if (id == 0x3E0){  //Haltech Protocol
    coolantTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Coolant Temp:");
    // Serial.println(coolantTempCAN/10);   // y = (x/10)-273.15 , KELVIN

    airTempCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("IAT:");
    // Serial.println(airTempCAN/10);   // y = (x/10)-273.15 , KELVIN

    fuelTempCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Fuel Temp:");
    // Serial.println(fuelTempCAN/10);   // y = x/10

    oilTempCAN = (rxBuf[6]<<8) + rxBuf[7];
    // Serial.print("Oil Temp:");
    // Serial.println(oilTempCAN/10);   // y = x/10
  }
  else if (id == 0x3E1){  //Haltech Protocol
    transTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Trans Temp:");
    // Serial.println(transTempCAN/10);   // y = x/10

    fuelCompCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Ethanol %:");
    // Serial.println(fuelCompCAN/10);   // y = x/10
  }

  
}