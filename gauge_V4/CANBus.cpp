/*
 * ========================================
 * CAN BUS FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of CAN bus communication functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "CANBus.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"

void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        // LITTLE ENDIAN byte packing
        // Word 1 (bytes 0-1)
        data[0] = lowByte(inputVal_1);   // LSB first
        data[1] = highByte(inputVal_1);  // MSB second
        // Word 2 (bytes 2-3)
        data[2] = lowByte(inputVal_2);
        data[3] = highByte(inputVal_2);
        // Word 3 (bytes 4-5)
        data[4] = lowByte(inputVal_3);
        data[5] = highByte(inputVal_3);
        // Word 4 (bytes 6-7)
        data[6] = lowByte(inputVal_4);
        data[7] = highByte(inputVal_4);

        //Serial.println(inputVal_1);  // Debug output
        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);  // Send 8-byte message, standard ID
}

/**
 * sendCAN_BE - Send CAN message with Big Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Big Endian: High byte first, then low byte (Motorola byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x12 (high byte)
 *   data[1] = 0x34 (low byte)
 * 
 * Used for: Speed data (compatible with Haltech protocol)
 */

void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        // BIG ENDIAN byte packing
        // Word 1 (bytes 0-1)
        data[0] = highByte(inputVal_1);  // MSB first
        data[1] = lowByte(inputVal_1);   // LSB second
        // Word 2 (bytes 2-3)
        data[2] = highByte(inputVal_2);
        data[3] = lowByte(inputVal_2);
        // Word 3 (bytes 4-5)
        data[4] = highByte(inputVal_3);
        data[5] = lowByte(inputVal_3);
        // Word 4 (bytes 6-7)
        data[6] = highByte(inputVal_4);
        data[7] = lowByte(inputVal_4);

        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);  // Send 8-byte message, standard ID
}

/**
 * receiveCAN - Read CAN message from receive buffer
 * 
 * Reads a CAN message from the MCP2515 controller's receive buffer.
 * Called when CAN interrupt pin goes low (message waiting).
 * Message data is copied to canMessageData[] for parsing.
 * 
 * Global variables modified:
 * - rxId: CAN message identifier
 * - len: Number of data bytes (0-8)
 * - rxBuf: Raw message data bytes
 * - canMessageData: Copy of message data for parsing
 * 
 * The commented-out debug code can be enabled to print CAN messages to serial.
 */

void receiveCAN ()
{
  
    CAN0.readMsgBuf(&rxId, &len, rxBuf);  // Read message: ID, length, and data bytes
    
    // Copy received data to processing buffer
    for (byte i =0; i< len; i++){
      canMessageData[i] = rxBuf[i];
      //Serial.println(canMessageData[i]);  // Debug: print each byte
    }
    
    // Debug code for printing CAN messages (currently disabled)
//    if((rxId & 0x80000000) == 0x80000000)     // Check if extended ID (29-bit)
//      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
//    else                                       // Standard ID (11-bit)
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


///// LED TACH AND SHIFT LIGHT FUNCTION /////