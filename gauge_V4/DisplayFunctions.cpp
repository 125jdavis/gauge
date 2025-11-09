/*
 * ========================================
 * DISPLAY FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of all OLED display rendering functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "DisplayFunctions.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"
#include "ImageData.h"

void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(20,8);
    display->println("SETTINGS");
    display->drawRect(0,0,128,32,SSD1306_WHITE);  // Draw border rectangle              
    display->display();
}

/**
 * dispDisp2Select - Display "DISPLAY 2" submenu header
 * Shows header when selecting what to display on second screen
 */

void dispDisp2Select (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(15,8);
    display->println("DISPLAY 2");                 
    display->display();
}

/**
 * dispUnits - Display "UNITS" submenu header
 * Shows header when selecting metric vs imperial units
 */

void dispUnits (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(32,8);
    display->println("UNITS");                 
    display->display();
}

/**
 * dispClockOffset - Display "SET CLOCK" header
 * Shows header when adjusting time zone offset
 */

void dispClockOffset (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(0,9);
    display->println("SET CLOCK");                 
    display->display();
}

/**
 * dispRPM - Display engine RPM
 * 
 * Shows large RPM value with "RPM" label.
 * Uses dynamic centering based on number of digits.
 * 
 * @param display - Pointer to display object (display1 or display2)
 */

void dispRPM (Adafruit_SSD1306 *display){
    byte nDig = digits(RPM);  // Calculate number of digits for centering
    byte center = 47;         // Center point for display
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(3);  // Large text for main value
    display->setCursor(center-((nDig*18)/2),6);  // Center based on digits (18 pixels per char at size 3)
    display->println(RPM); 
    display->setTextSize(2);  // Smaller text for label
    display->setCursor(88,10);
    display->println("RPM");                
    display->display();
}

/**
 * dispSpd - Display vehicle speed
 * 
 * Shows speed with automatic unit conversion based on 'units' setting.
 * Metric: km/h (from GPS data)
 * Imperial: mph (converted from km/h)
 * 
 * Uses dynamic centering based on number of digits.
 * 
 * @param display - Pointer to display object (display1 or display2)
 * 
 * Note: spd is stored as km/h * 100 for integer precision
 */

void dispSpd (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();

    if (units == 0){    // Metric Units (km/h)
      float spdDisp = spd*0.01;  // Convert from km/h*100 to km/h
      byte nDig = digits(spdDisp);  // Get number of digits for centering
      byte center = 37;
      display->setTextSize(3);  // Large text for speed value (18 pixels per character)
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  // Print without decimal places
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);  // Smaller text for units
      display->println("km/h");
               
    } 
    else {              // Imperial Units (mph)
      float spdDisp = spd * 0.006213711922;  // Convert km/h*100 to mph (factor = 0.6213712 / 100)
      byte nDig = digits (spdDisp);
      byte center = 47;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  // Print without decimal places
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);
      display->println("MPH");          
    }
          
    display->display();
}

/**
 * dispOilTemp - Display oil temperature with icon
 * 
 * Shows oil temp with unit conversion and degree symbol.
 * Displays oil can/thermometer icon on left side.
 * 
 * Metric: Celsius (no conversion needed)
 * Imperial: Fahrenheit (C * 1.8 + 32)
 * 
 * @param display - Pointer to display object
 */

void dispOilTemp (Adafruit_SSD1306 *display) {
    float oilTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->drawBitmap(0, 0, img_oilTemp, 40, 32, 1);  // Draw oil/temp icon (40x32 pixels)
    byte center = 71;  // Center point for text (offset for icon on left)
    
    if (units == 0){    // Metric Units (Celsius)
      oilTempDisp = oilTemp;  // No conversion needed - already in Celsius
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);  // Print temperature value
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);  // Draw degree symbol (small circle)
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");  // Celsius label
    }

    else {              // Imperial Units (Fahrenheit)
      oilTempDisp = (oilTemp*1.8) + 32;  // Convert Celsius to Fahrenheit
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);  // Draw degree symbol
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");  // Fahrenheit label
    }

    display->display();
}

/**
 * dispFuelPrs - Display fuel pressure
 * 
 * Shows fuel pressure with "FUEL PRESSURE" label and unit conversion.
 * Includes protection against negative values (sensor error or not connected).
 * 
 * Metric: bar (kPa / 100)
 * Imperial: PSI (kPa * 0.1450377)
 * 
 * @param display - Pointer to display object
 * 
 * Note: fuelPrs is gauge pressure in kPa (atmospheric pressure already subtracted)
 */

void dispFuelPrs (Adafruit_SSD1306 *display) {
    float fuelPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2); 
    display->setCursor(0,3);
    display->println("FUEL");  // Label line 1
    display->setTextSize(1); 
    display->setCursor(0,21);
    display->println("PRESSURE");  // Label line 2

    if (units == 0){    // Metric Units (bar)
      fuelPrsDisp = fuelPrs/100;  // Convert kPa to bar (1 bar = 100 kPa)
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}  // Clamp to 0 if negative
      byte nDig = 3;  // Always 3 digits for bar (e.g., "3.5")
      byte center = 79;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 1);  // Print with 1 decimal place
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // Imperial Units (PSI)
      fuelPrsDisp = fuelPrs * 0.1450377;  // Convert kPa to PSI (1 kPa = 0.145 PSI)
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}  // Clamp to 0 if negative
      byte nDig = digits (fuelPrsDisp);
      byte center = 71;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 0);  // Print without decimal
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
    
    display->display();
}

/**
 * dispFuelComp - Display fuel composition (ethanol percentage)
 * 
 * Shows flex fuel percentage (0-100% ethanol).
 * Useful for E85 flex fuel vehicles to know fuel composition.
 * 
 * @param display - Pointer to display object
 * 
 * Example: E85 would show as 85%
 */

void dispFuelComp (Adafruit_SSD1306 *display) {
    byte nDig = digits (fuelComp);
    byte center = 79;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(2,0);
    display->println("Flex");  // Label line 1
    display->setCursor(2,15);
    display->println("Fuel");  // Label line 2
    display->setTextSize(3); 
    display->setCursor(center-((nDig*18)/2),6);
    display->print(fuelComp, 0);  // Print percentage value
    display->println("%");        
    display->display();
}

/**
 * dispAFR - Display Air/Fuel Ratio
 * 
 * Shows AFR value from wideband O2 sensor.
 * Important for tuning - stoichiometric AFR for gasoline is ~14.7
 * 
 * @param display - Pointer to display object
 */

void dispAFR (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setCursor(8,6);
    display->setTextSize(3); 
    display->print(afr, 1);  // Print AFR with 1 decimal place (e.g., 14.7)
    display->setCursor(88,10);
    display->setTextSize(2);
    display->println("AFR");         
    display->display();
}

/**
 * dispFalconScript - Display Falcon logo splash screen
 * Simple bitmap display - shows Falcon script logo
 */

void dispFalconScript(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_falcon_script, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302CID - Display 302 CID engine badge
 * Shows "302 CID" (Cubic Inch Displacement) logo
 */

void disp302CID(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_302_CID, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302V - Display 302 V8 engine badge
 * Shows "302V" (V8) logo with graphic
 */

void disp302V(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_302V, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * dispOilPrsGfx - Display oil pressure with icon
 * 
 * Similar to dispFuelPrs but with oil can icon.
 * Shows oil pressure with unit conversion.
 * 
 * Metric: bar (kPa / 100)
 * Imperial: PSI (kPa * 0.1450377)
 * 
 * @param display - Pointer to display object
 * 
 * Note: Negative values clamped to 0 (sensor error or engine off)
 */

void dispOilPrsGfx (Adafruit_SSD1306 *display) {
    float oilPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->drawBitmap(0, 0, img_oilPrs, 40, 32, 1);  // Draw oil can icon
    if (oilPrs < 0) {oilPrs = 0;}  // Clamp negative values
    
    if (units == 0){    // Metric Units (bar)
      oilPrsDisp = oilPrs/100;  // Convert kPa to bar
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = 3;  // Always 3 digits for bar
      byte center = 79;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 1);  // Print with 1 decimal
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // Imperial Units (PSI)
      oilPrsDisp = oilPrs * 0.1450377;  // Convert kPa to PSI
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = digits (oilPrsDisp);
      byte center = 71;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
          
    display->display();
}


void dispOilTempGfx (Adafruit_SSD1306 *display) {
    float oilTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_oilTemp, 40, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      oilTempDisp = oilTemp;
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");
    }

    else {              // 'Merican Units
      oilTempDisp = (oilTemp*1.8) + 32; // convert C to F
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");
    }

    display->display();
}


void dispCoolantTempGfx (Adafruit_SSD1306 *display) {
    float coolantTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_coolantTemp, 38, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      coolantTempDisp = coolantTemp;
      byte nDig = digits (coolantTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(coolantTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");
    }

    else {              // 'Merican Units
      coolantTempDisp = (coolantTemp*1.8) + 32; // convert C to F
      byte nDig = digits (coolantTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(coolantTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");
    }

    display->display();
}


void dispBattVoltGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_battVolt, 35, 32, 1);
    display->setTextSize(3); 
    display->setCursor(42,6);
    display->println(vBatt, 1);
    display->setTextSize(2);
    display->setCursor(116,12); 
    display->println("V");         
    display->display();
}


void dispFuelLvlGfx (Adafruit_SSD1306 *display) {
    float fuelLvlDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_fuelLvl, 32, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      fuelLvlDisp = fuelLvl*3.785; // convert to liters
      byte nDig = digits(fuelLvlDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelLvlDisp, 0);
      display->setCursor(center+((nDig*18)/2)+4,6);
      display->println("l");
    }

    else {              // 'Merican Units
      fuelLvlDisp = fuelLvl; // read in gallons
      byte nDig = digits(fuelLvlDisp) +2 ;
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelLvlDisp, 1);
      display->setCursor(center+((nDig*18)/2)+2,18);
      display->setTextSize(1); 
      display->println("gal");
    }

    display->display();
}


void dispTripOdo (Adafruit_SSD1306 *display) {
    float odoDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
        
    if (units == 0){    // Metric Units
      odoDisp = odoTrip; 
      display->setCursor(100,6);
      display->setTextSize(2);
      display->println("km");         
    } 
    else {              // 'Merican units
      odoDisp = odoTrip * 0.6213712; //convert km to miles  
      display->setCursor(100,6);
      display->setTextSize(2);
      display->println("mi");          
    }

    display->setCursor(35,6);
    display->setTextSize(2); 
    // right justify
    if (odoDisp < 10) {
      display->setTextColor(BLACK); 
      display->print("00");
    }
    else if (odoDisp < 100){
      display->setTextColor(BLACK); 
      display->print("0");
    }
    
    display->setTextColor(WHITE);
    // remove tenths once 1000 is reached
    if (odoDisp < 1000) { 
      display->println(odoDisp, 1);
    }
    else {
      display->println(odoDisp, 0);
    }
    
    display->setTextSize(1);
    display->setCursor(1,3);
    display->println("Trip");
    display->setCursor(1,13);
    display->println("Odo:"); 
    display->display();  
}


void dispOdoResetYes(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);
    display->setCursor(5,0);
    display->println("RESET ODO?");
    display->fillRect(13,15,38,16,1);
    display->setCursor(15,16);
    display->setTextColor(BLACK); 
    display->println("YES");
    display->setCursor(76,16);
    display->setTextColor(WHITE); 
    display->println("NO");
    display->display();
}


void dispOdoResetNo(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);
    display->setCursor(5,0);
    display->println("RESET ODO?");
    display->setCursor(15,16);
    display->setTextColor(WHITE); 
    display->println("YES");
    display->fillRect(74,15,26,16,1);
    display->setCursor(76,16);
    display->setTextColor(BLACK); 
    display->println("NO");
    display->display();
}


void dispIgnAng (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(6,0);
    display->println("IGN");
    display->setCursor(2,15);
    display->println("BTDC");            
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->print(ignAngCAN/10); 
    display->write(0xF7);  
    display->println();      
    display->display();
}


void dispInjDuty (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(6,0);
    display->println("INJ");
    display->setCursor(2,15);
    display->println("DUTY");            
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->print(injDutyCAN/10);  
    display->println("%");      
    display->display();
}

/**
 * dispClock - Display time from GPS with local offset
 * 
 * Shows current time in HH:MM format using GPS time + clock offset.
 * GPS provides UTC time, clock offset adjusts for local timezone.
 * 
 * @param display - Pointer to display object
 * 
 * Time handling:
 * - GPS hour is in UTC (0-23)
 * - clockOffset is added to get local time
 * - Wraps around at 24 hours
 * - Minutes are zero-padded (e.g., "3:05" not "3:5")
 */

void dispClock (Adafruit_SSD1306 *display){
    byte hourAdj;
    display->clearDisplay();
    
    // Calculate local hour from UTC + offset with wraparound
    if (clockOffset + hour > 23) {        
      hourAdj = clockOffset + hour - 24;  // Wrap to next day
    }
    else {
      hourAdj = clockOffset + hour;
    }

    byte nDig = digits(hourAdj)+3;  // +3 for colon and 2-digit minutes
    byte center = 63;
    
    display->setTextColor(WHITE);
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2),6);
    display->print(hourAdj); 
    display->print(':');
    if (minute < 10) { display->print('0'); }  // Zero-pad minutes (e.g., "03" not "3")
    display->println(minute);
    display->display();
}

/**
 * digits - Count number of digits in a number
 * 
 * Helper function for dynamic text centering on displays.
 * Determines how many character widths are needed for a number.
 * 
 * @param val - Number to count digits for (can be negative)
 * @return Number of digits (1-4), includes sign for negative numbers
 * 
 * Example: 
 * - digits(5) = 1
 * - digits(42) = 2
 * - digits(-7) = 2 (includes minus sign)
 */
byte digits(float val){
  byte nDigits;
  if (val >= 0){ 
    if (val < 10)         {nDigits = 1;}
    else if (val < 100)   {nDigits = 2;}
    else if (val < 1000)  {nDigits = 3;}
    else if (val < 10000) {nDigits = 4;}
  }
  else {  // Negative numbers - count includes minus sign
    if (val > -10)        {nDigits = 2;}  // "-" + 1 digit
    else if (val > -100)  {nDigits = 3;}  // "-" + 2 digits
    else if (val > -1000) {nDigits = 4;}  // "-" + 3 digits
  }
  return nDigits;
}

/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * Handle CAN bus communication with Haltech ECU and other modules
 * CAN bus operates at 500kbps with standard 11-bit identifiers
 */

/**
 * sendCAN_LE - Send CAN message with Little Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Little Endian: Low byte first, then high byte (Intel byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x34 (low byte)
 *   data[1] = 0x12 (high byte)
 * 
 * Used for: Sensor data to other modules (thermistor, fuel level, baro)
 */