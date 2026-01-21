/*
 * ========================================
 * CAN BUS FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "can.h"
#include "globals.h"

/**
 * sendCAN_LE - Send CAN message with Little Endian byte order
 */
void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        byte data[8];  // Local buffer for CAN message (8 bytes)
        
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
 */
void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        byte data[8];  // Local buffer for CAN message (8 bytes)
        
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
        data[7] = highByte(inputVal_4);

        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);  // Send 8-byte message, standard ID
}

/**
 * receiveCAN - Read CAN message from receive buffer
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

/**
 * parseCAN - Parse received CAN message based on ID
 */
void parseCAN( unsigned long id, unsigned long msg)
{
  // Dispatch to appropriate protocol parser based on configuration
  switch (CAN_PROTOCOL) {
    case CAN_PROTOCOL_HALTECH_V2:
      parseCANHaltechV2(id);
      break;
    case CAN_PROTOCOL_MEGASQUIRT:
      parseCANMegasquirt(id);
      break;
    case CAN_PROTOCOL_AIM:
      parseCANAim(id);
      break;
    case CAN_PROTOCOL_OBDII:
      parseCANOBDII(id);
      break;
    default:
      // Unknown protocol, do nothing
      break;
  }
}

/**
 * parseCANHaltechV2 - Parse CAN messages using Haltech v2 protocol
 * 
 * Reference: Haltech CAN Broadcast Protocol V2.35.0
 * All values are Big Endian (MSB first)
 */
void parseCANHaltechV2(unsigned long id)
{
  if (id == 0x301) {  //test 
    pumpPressureCAN = (rxBuf[0]<<8) + rxBuf[1];
  }
  else if (id == 0x360){  // RPM, MAP, TPS
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];  // Engine RPM (direct value)
    mapCAN = (rxBuf[2]<<8) + rxBuf[3];  // MAP in kPa * 10
    tpsCAN = (rxBuf[4]<<8) + rxBuf[5];  // TPS in % * 10
  }
  else if (id == 0x361){  // Fuel Pressure, Oil Pressure
    fuelPrsCAN = (rxBuf[0]<<8) + rxBuf[1];  // Fuel pressure in kPa * 10
    oilPrsCAN = (rxBuf[2]<<8) + rxBuf[3];   // Oil pressure in kPa * 10
  }
  else if (id == 0x362){  // Injector Duty, Ignition Angle
    injDutyCAN = (rxBuf[0]<<8) + rxBuf[1];  // Injector DC in % * 10
    ignAngCAN = (rxBuf[4]<<8) + rxBuf[5];   // Ignition angle in deg * 10
  }
  else if (id == 0x368){  // Lambda (AFR)
    afr1CAN = (rxBuf[0]<<8) + rxBuf[1];     // Lambda * 1000
  }
  else if (id == 0x369){  // Knock Level
    knockCAN = (rxBuf[0]<<8) + rxBuf[1];    // Knock level
  }
  else if (id == 0x3E0){  // Temperatures
    coolantTempCAN = (rxBuf[0]<<8) + rxBuf[1];  // Coolant temp in K * 10
    airTempCAN = (rxBuf[2]<<8) + rxBuf[3];      // Air temp in K * 10
    fuelTempCAN = (rxBuf[4]<<8) + rxBuf[5];     // Fuel temp in K * 10
    oilTempCAN = (rxBuf[6]<<8) + rxBuf[7];      // Oil temp in C * 10
  }
  else if (id == 0x3E1){  // Trans Temp, Fuel Composition
    transTempCAN = (rxBuf[0]<<8) + rxBuf[1];    // Trans temp in C * 10
    fuelCompCAN = (rxBuf[4]<<8) + rxBuf[5];     // Ethanol % * 10
  }
}

/**
 * parseCANMegasquirt - Parse CAN messages using Megasquirt protocol
 * 
 * Reference: Megasquirt CAN Broadcast Protocol
 * Base ID can vary, typically 0x5F0 - 0x5FF
 * All values are Little Endian (LSB first)
 */
void parseCANMegasquirt(unsigned long id)
{
  // Megasquirt uses base ID + offset structure
  // Default base is 0x5F0, but can be configured
  // Using offsets from base:
  
  if (id == 0x5F0) {  // Offset 0: MAP, RPM
    // MAP: bytes 0-1 (kPa * 10)
    mapCAN = rxBuf[0] + (rxBuf[1]<<8);  // Little Endian
    // RPM: bytes 2-3
    rpmCAN = rxBuf[2] + (rxBuf[3]<<8);  // Little Endian
  }
  else if (id == 0x5F1) {  // Offset 1: Coolant Temp, CLT correction
    // Coolant temp: bytes 0-1 (deg F * 10, need conversion)
    int tempF = rxBuf[0] + (rxBuf[1]<<8);
    coolantTempCAN = (int)(((tempF / 10.0) - 32.0) * 5.0/9.0 * 10.0 + 2731.5);  // Convert F to K*10
  }
  else if (id == 0x5F2) {  // Offset 2: TPS, Battery voltage
    // TPS: bytes 0-1 (% * 10)
    tpsCAN = rxBuf[0] + (rxBuf[1]<<8);
  }
  else if (id == 0x5F3) {  // Offset 3: AFR1, AFR2
    // AFR1: bytes 0-1 (AFR * 10, convert to * 1000)
    int afr = rxBuf[0] + (rxBuf[1]<<8);
    afr1CAN = afr * 100;  // Convert from *10 to *1000
  }
  else if (id == 0x5F4) {  // Offset 4: Knock, ego correction
    // Knock: bytes 0-1
    knockCAN = rxBuf[0] + (rxBuf[1]<<8);
  }
  // Note: Megasquirt doesn't broadcast Oil Pressure, Oil Temp, Fuel Pressure by default
  // These would need to be configured as custom channels if available
}

/**
 * parseCANAim - Parse CAN messages using AiM protocol
 * 
 * Reference: AiM CAN Protocol
 * Standard AiM messages use specific IDs
 */
void parseCANAim(unsigned long id)
{
  // AiM uses different message IDs for different channels
  // Common channels:
  
  if (id == 0x0B0) {  // RPM and Speed
    // RPM: bytes 0-1 (Big Endian)
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Speed: bytes 2-3 (km/h * 10, Big Endian)
    int spdKmh = (rxBuf[2]<<8) + rxBuf[3];
    // Store for potential use (currently not used for speedometer in this system)
  }
  else if (id == 0x0B1) {  // Temperatures
    // Coolant temp: bytes 0-1 (deg C * 10, Big Endian)
    int coolantC = (rxBuf[0]<<8) + rxBuf[1];
    coolantTempCAN = coolantC + 2731;  // Convert C*10 to K*10
    // Oil temp: bytes 2-3 (deg C * 10, Big Endian)
    oilTempCAN = (rxBuf[2]<<8) + rxBuf[3];
  }
  else if (id == 0x0B2) {  // Pressures
    // MAP: bytes 0-1 (mbar, Big Endian) - convert to kPa*10
    int mapMbar = (rxBuf[0]<<8) + rxBuf[1];
    mapCAN = mapMbar / 10;  // mbar to kPa*10 (1 kPa = 10 mbar)
    // Oil pressure: bytes 2-3 (bar * 10, Big Endian) - convert to kPa*10
    int oilBar = (rxBuf[2]<<8) + rxBuf[3];
    oilPrsCAN = oilBar * 10;  // bar*10 to kPa*10
    // Fuel pressure: bytes 4-5 (bar * 10, Big Endian) - convert to kPa*10
    int fuelBar = (rxBuf[4]<<8) + rxBuf[5];
    fuelPrsCAN = fuelBar * 10;  // bar*10 to kPa*10
  }
  else if (id == 0x0B3) {  // Lambda
    // Lambda: bytes 0-1 (lambda * 1000, Big Endian)
    afr1CAN = (rxBuf[0]<<8) + rxBuf[1];
  }
  // Note: AiM protocol may vary based on ECU configuration
  // These are common default mappings
}

/**
 * parseCANOBDII - Parse OBDII response messages
 * 
 * OBDII responses come from ID 0x7E8 (or 0x7E8-0x7EF for multi-ECU)
 * Response format: [length, 0x41, PID, data...]
 */
void parseCANOBDII(unsigned long id)
{
  // Check if this is an OBDII response (0x7E8 - 0x7EF)
  if (id < 0x7E8 || id > 0x7EF) {
    return;  // Not an OBDII response
  }
  
  // Verify it's a response to our request (mode 0x41 = response to mode 0x01 request)
  if (rxBuf[1] != 0x41) {
    return;  // Not a mode 0x01 response
  }
  
  uint8_t pid = rxBuf[2];  // PID being responded to
  
  switch (pid) {
    case 0x0C:  // Engine RPM
      // Formula: ((A*256)+B)/4
      rpmCAN = ((rxBuf[3] << 8) + rxBuf[4]) / 4;
      obdiiAwaitingResponse = false;
      break;
      
    case 0x0D:  // Vehicle Speed
      // Formula: A (km/h)
      // Store as km/h * 100 for compatibility
      // Note: This updates a different variable, as spdCAN is for output
      // Speed input typically comes from Hall sensor or GPS in this system
      obdiiAwaitingResponse = false;
      break;
      
    case 0x05:  // Engine Coolant Temperature
      // Formula: A - 40 (deg C)
      int coolantC = rxBuf[3] - 40;
      coolantTempCAN = (coolantC + 273) * 10;  // Convert to K*10
      obdiiAwaitingResponse = false;
      break;
      
    case 0x0B:  // Intake Manifold Absolute Pressure
      // Formula: A (kPa)
      mapCAN = rxBuf[3] * 10;  // Convert to kPa*10
      obdiiAwaitingResponse = false;
      break;
      
    case 0x24:  // Oxygen Sensor 1 Lambda (if available)
      // Formula: ((A*256)+B) * 0.0000305 (lambda)
      // Convert to lambda * 1000
      unsigned int lambdaRaw = (rxBuf[3] << 8) + rxBuf[4];
      afr1CAN = (int)(lambdaRaw * 0.0305);  // Result is lambda * 1000
      obdiiAwaitingResponse = false;
      break;
      
    // Note: OBDII doesn't typically provide Oil Pressure, Oil Temp, Fuel Pressure
    // These parameters are not available via standard OBDII PIDs
    
    default:
      obdiiAwaitingResponse = false;
      break;
  }
}

/**
 * sendOBDIIRequest - Send OBDII PID request
 * 
 * Sends a mode 0x01 (show current data) request for a specific PID
 * Request ID: 0x7DF (functional broadcast)
 * Format: [0x02, 0x01, PID, 0x00, 0x00, 0x00, 0x00, 0x00]
 */
void sendOBDIIRequest(uint8_t pid)
{
  byte data[8] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
  CAN0.sendMsgBuf(0x7DF, 0, 8, data);
  obdiiAwaitingResponse = true;
  obdiiCurrentPID = pid;
}

/**
 * pollOBDII - Manage OBDII polling for priority 1 and 2 parameters
 * 
 * Priority 1 (10Hz): Vehicle Speed (0x0D), RPM (0x0C), Lambda (0x24), MAP (0x0B)
 * Priority 2 (1Hz): Coolant Temp (0x05)
 * 
 * Rotates through PIDs to avoid overwhelming the ECU
 */
void pollOBDII()
{
  static uint8_t priority1Index = 0;  // Current priority 1 PID index
  const uint8_t priority1PIDs[] = {0x0D, 0x0C, 0x24, 0x0B};  // Speed, RPM, Lambda, MAP
  const uint8_t priority1Count = 4;
  
  // Don't send new request if still waiting for response
  if (obdiiAwaitingResponse) {
    return;
  }
  
  // Priority 1 polling at 10Hz (100ms interval)
  if (millis() - timerOBDIIPriority1 >= 100) {
    sendOBDIIRequest(priority1PIDs[priority1Index]);
    priority1Index = (priority1Index + 1) % priority1Count;  // Rotate through PIDs
    timerOBDIIPriority1 = millis();
  }
  
  // Priority 2 polling at 1Hz (1000ms interval)
  if (millis() - timerOBDIIPriority2 >= 1000) {
    sendOBDIIRequest(0x05);  // Coolant temp
    timerOBDIIPriority2 = millis();
  }
}

