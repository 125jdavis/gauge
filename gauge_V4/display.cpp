#include "display.h"

// Reads Encoder Switch and debounces
void swRead() {       
  stateSW = digitalRead(SWITCH);            // read the digital input pin
  int stateChange = stateSW - lastStateSW;  // calculate state change value

  if ((millis() - lastStateChangeTime) > debounceDelay) {
    debounceFlag = 0;  // flag will block state change if debounce time has not elapsed
  }

  if (stateChange < 0 && debounceFlag == 0) {  //if state change negative, button has been presed
    lastStateChangeTime = millis();            // reset the debouncing timer
    debounceFlag = 1;                          // set flag to block button bounce
  } else if (stateChange > 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();    // reset the debouncing timer
    debounceFlag = 1;                  // set flag to block button bounce
    button = 1;
  } else if (stateChange = 0) {  // if state change = 0, nothing has happened
  }
  lastStateSW = stateSW;  // saves current switch state for next time
}

// General Encoder Incrementer for menu navigation 
void rotate() {
  //void rotate() {
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    if (dispArray1[menuLevel] == nMenuLevel) dispArray1[menuLevel] = 0; else dispArray1[menuLevel]++;  // increment up one within range given for current menu level
  } else if (result == DIR_CCW) {
    if (dispArray1[menuLevel] == 0l) dispArray1[menuLevel] = nMenuLevel; else dispArray1[menuLevel]--;  // increment down one within range given for current menu level
  }
}

// Controls display and menu navigation
void dispMenu() {
  switch (dispArray1[0]) {  // Level 0
    case 1:                //dispArray1 {1 0 0 0} Oil Pressure
      if (menuLevel == 0 && button == 1) {  //if button is pressed, go down one level
        button = 0; // reset button state
      }
      // Serial.println("Oil Pressure");
      dispOilPrsGfx(&display1);
      break;
    
    case 2:                //dispArray1 {2 0 0 0} Coolant Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Coolant Temp");
      dispCoolantTempGfx(&display1);
      break;
    
    case 3:                //dispArray1 {3 0 0 0} Oil Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state     
      }
      // Serial.println("Oil Temp");
      dispOilTempGfx(&display1);
      break;
    
    case 4:                //dispArray1 {4 0 0 0} Fuel Level
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Fuel Level");
      dispFuelLvlGfx(&display1);
      break;
    
    case 5:               //dispArray1 {5 0 0 0} Battery Voltage
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("batt voltage");
      dispBattVoltGfx(&display1);
      break;

    case 6:               //dispArray1 {6 0 0 0} Clock
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Clock");
      dispClock(&display1);
      break;  

    case 7:                //dispArray1 {7 0 0 0} TripOdometer
	  if (menuLevel == 0 && button == 1) {  //if button is pressed, change menu level
        button = 0;
        menuLevel = 1; // menu level 1 (0 indexed)
        nMenuLevel = 1; // number of screens on this level - 1 (0 indexed)
      } 
      else if (menuLevel == 0) {  //if no button is pressed, display trip odometer
        dispTripOdo(&display1);
        // Serial.println("Trip Odo");
      } 
      else {  // proceed to Level 0 screen 3 deeper levels
        switch (dispArray1[1]) {
          case 0: //Select screen for display2                  dispArray1 {0 0 0 0}
            dispOdoResetYes(&display1); // display odo reset: yes
			      if (button == 1) {
			        odoTrip = 0;
			        goToLevel0();
			        dispArray1[0] = 7;
            } 
            break;
		      case 1:
			      dispOdoResetNo(&display1); // display odo reset: no
			      if (button == 1) {
				      goToLevel0();
				      dispArray1[0] = 7;
			      } 
          break;
		    } 
	    }
      break;    
    
    case 8:                //dispArray1 {8 0 0 0} Speed
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Speed");
      dispSpd(&display1);
      break;  
    
    case 9:                //dispArray1 {9 0 0 0} RPM
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("RPM");
      dispRPM(&display1);
      break;  
    
    case 10:                //dispArray1 {10 0 0 0} Ignition Timing
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("ignition timing");
      dispIgnAng(&display1);
      break;
    
    case 11:                //dispArray1 {11 0 0 0} AFR
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("AFR");
      dispAFR(&display1);
      break;  
    
    case 12:               //dispArray1 {12 0 0 0} Fuel Pressure
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Fuel Pressure");
      dispFuelPrs(&display1);
      break;  

    case 13:               //dispArray1 {13 0 0 0} Fuel Composition
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("ethanol %");
      dispFuelComp(&display1);
      break;  

    case 14:               //dispArray1 {14 0 0 0} INJ DUTY
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        button = 0; // reset button state
      }
      // Serial.println("Inj Duty");
      dispInjDuty(&display1);
      break;
      
    case 15:               //dispArray1 {15 0 0 0} Falcon Script
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        //goToLevel0();
      }
      // Serial.println("falcon Script");
      dispFalconScript(&display1);
      break;  

    case 0:                //dispArray1 {0 0 0 0} Settings // ALWAYS LAST SCREEN, ALWAYS CASE 0 //

      if (menuLevel == 0 && button == 1) {  //if button is pressed, change menu level
        button = 0;
        menuLevel = 1; // menu level 1 (0 indexed)
        nMenuLevel = 3 ; // number of screens on this level - 1 (0 indexed)
      } 
      else if (menuLevel == 0) {  //if no button is pressed, display settings
        // Serial.println("settings");
        dispSettings(&display1);
      } 
      else {  // proceed to Level 0 screen 3 deeper levels

        switch (dispArray1[1]) {
          case 0: //Select screen for display2                  dispArray1 {0 0 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;  // menu level 2 (0 indexed)
              nMenuLevel = 8; // number of screens on this level - 1 (0 indexed)
            } 
            else if (menuLevel == 1) {
              //Serial.println("Display 2");
              dispDisp2Select(&display1);
            } 
            else {
              switch (dispArray1[2]) {
                case 0:         // Oil Pressure on Display 2
                  //Serial.println("Disp2: Oil Pressure");
                  dispArray2[0] = 0;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Coolant Temp on Display 2
                  //Serial.println("Disp2: Coolant Temp");
                  dispArray2[0] = 1;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 2:         // Battery Voltage on Display 2
                  //Serial.println("Disp2: Battery Voltage");
                  dispArray2[0] = 2;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 3:         // Fuel Level on Display 2
                  //Serial.println("Disp2: Fuel Level");
                  dispArray2[0] = 3;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 4:         // RPM on Display 2
                  //Serial.println("Disp2: RPM");
                  dispArray2[0] = 4;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 5:         // Speed on Display 2
                  //Serial.println("Disp2: Speed");
                  dispArray2[0] = 5;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;

                case 6:         // Clock on Display 2
                  //Serial.println("Clock");
                  dispArray2[0] = 6;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;

                case 7:         // 302CID on Display 2
                  //Serial.println("Disp2: 302CID");
                  dispArray2[0] = 7;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 8:         // 302V on Display 2
                  //Serial.println("Disp2: 302V");
                  dispArray2[0] = 8;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 9:         // Falcon Script on Display 2
                  //Serial.println("Disp2: Falcon Script");
                  dispArray2[0] = 9;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                
              }
            }
            break;
 
          case 1: //Select units to display                  dispArray1 {0 1 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;
              nMenuLevel = 1;
            } else if (menuLevel == 1) {
              //Serial.println("Units");
              dispUnits(&display1);
            } else {
              switch (dispArray1[2]) {
                case 0:         // Metric Units
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();             //clear buffer
                  display1.setTextSize(2);             // text size
                  display1.setCursor(31,8);
                  display1.println("Metric");                 
                  display1.display();
                  units = 0;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Merican Units
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();             //clear buffer
                  display1.setTextSize(2);             // text size
                  display1.setCursor(20,8);
                  display1.println("'Merican");                 
                  display1.display();
                  units = 1;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
              }
            }
            break;

          case 2: //Clock Offset for modifying from GMT   dispArray1 {0 2 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;

            } else if (menuLevel == 1) {
              // Serial.println("ClockOffset");
              dispClockOffset(&display1);
            } else {
              if (button == 1) {
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, rotate, CHANGE);
                attachInterrupt(1, rotate, CHANGE);
                EEPROM.write(clockOffset, clockOffsetAddress);
                goToLevel0();
              } else {
                
                //Need code here to make the rotary encoder change the offset value
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, incrementOffset, CHANGE);
                attachInterrupt(1, incrementOffset, CHANGE);
                dispClock(&display1);

              }
            }
            break;
          
          case 3: // Exit from settings,                  dispArray1 {0 3 0 0}
            display1.setTextColor(WHITE); 
            display1.clearDisplay();             //clear buffer
            display1.setTextSize(2);             // text size
            display1.setCursor(35,8);
            display1.println("EXIT");                 
            display1.display();
            if (button == 1) {
                    goToLevel0();
            }
            break;
        }
        break;
      }
  }
}

// Navigation subfunction
void goToLevel0(void){
  button = 0;
  dispArray1[0] = 0;
  dispArray1[1] = 0;
  dispArray1[2] = 0;
  menuLevel = 0;
  nMenuLevel = 14;
}

// Display #2 Control function
void disp2(void){
  switch (dispArray2[0]){

    case 0: // Oil Pressure
      dispOilPrsGfx(&display2);
      break;
    
    case 1: // Coolant Temp
      dispCoolantTempGfx(&display2);
      break;

    case 2: // Battery Voltage
      dispBattVoltGfx(&display2);
      break;

    case 3: // Fuel Level
      dispFuelLvlGfx(&display2);
      break;

    case 4: // RPM
      dispRPM(&display2);
      break;

    case 5: // Speed
      dispSpd(&display2);
      break;

    case 6: // Clock
      dispClock(&display2);
      break;
    
    case 7: // 302CID
      disp302CID(&display2);
      break;

    case 8: // 302V
      disp302V(&display2);
      break;

    case 9: // Falcon Script
      dispFalconScript(&display2);
      break;
  }
}

// Increments Clock Offset
void incrementOffset() {
  //void rotate() {
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    if (clockOffset == 23) clockOffset = 0; else clockOffset++; // increment up one within range of 0-23
  } else if (result == DIR_CCW) {
    if(clockOffset == 0) clockOffset = 23; else clockOffset--;// increment down one within range of 0-23
  }
}


///// SCREEN DRAWING FUNCTIONS /////
void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(20,8);
    display->println("SETTINGS");
    display->drawRect(0,0,128,32,SSD1306_WHITE);                  
    display->display();
}

void dispDisp2Select (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(15,8);
    display->println("DISPLAY 2");                 
    display->display();
}

void dispUnits (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(32,8);
    display->println("UNITS");                 
    display->display();
}

void dispClockOffset (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(0,9);
    display->println("SET CLOCK");                 
    display->display();
}

void dispRPM (Adafruit_SSD1306 *display){
    byte nDig = digits(RPM);
    byte center = 47;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2),6);
    display->println(RPM); 
    display->setTextSize(2);             // text size
    display->setCursor(88,10);
    display->println("RPM");                
    display->display();
}

void dispSpd (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer

    if (units == 0){    // Metric Units
      float spdDisp = spd*0.01; // spd is km/h*100
      byte nDig = digits(spdDisp);
      byte center = 37;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2); 
      display->println("km/h");
               
    } 
    else {              // 'Merican units
      float spdDisp = spd * 0.006213711922; //convert km/h*100 to mph
      byte nDig = digits (spdDisp);
      byte center = 47;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);
      display->println("MPH");          
    }
          
    display->display();
}

void dispOilTemp (Adafruit_SSD1306 *display) {
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

void dispFuelPrs (Adafruit_SSD1306 *display) {
    float fuelPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2); 
    display->setCursor(0,3);
    display->println("FUEL");
    display->setTextSize(1); 
    display->setCursor(0,21);
    display->println("PRESSURE");

    if (units == 0){    // Metric Units
      fuelPrsDisp = fuelPrs/100; // convert kpa to bar
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}
      byte nDig = 3; //nDig always == 3 for metric oil pressure
      byte center = 79;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 1);
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // 'Merican units
      fuelPrsDisp = fuelPrs * 0.1450377; //convert kpa to PSI  
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}
      byte nDig = digits (fuelPrsDisp);
      byte center = 71;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
    
    display->display();
}

void dispFuelComp (Adafruit_SSD1306 *display) {
    byte nDig = digits (fuelComp);
    byte center = 79;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(2,0);
    display->println("Flex");
    display->setCursor(2,15);
    display->println("Fuel");            
    display->setTextSize(3); 
    display->setCursor(center-((nDig*18)/2),6);
    display->print(fuelComp, 0); 
    display->println("%");        
    display->display();
}

void dispAFR (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setCursor(8,6);
    display->setTextSize(3); 
    display->print(afr, 1);
    display->setCursor(88,10);
    display->setTextSize(2);
    display->println("AFR");         
    display->display();
}

void dispFalconScript(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_falcon_script, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void disp302CID(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_302_CID, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void disp302V(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_302V, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void dispOilPrsGfx (Adafruit_SSD1306 *display) {
    float oilPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_oilPrs, 40, 32, 1);
    if (oilPrs < 0) {oilPrs = 0;}
    
    if (units == 0){    // Metric Units
      oilPrsDisp = oilPrs/100; // convert kpa to bar
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = 3; //nDig always == 3 for metric oil pressure
      byte center = 79;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 1);
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // 'Merican units
      oilPrsDisp = oilPrs * 0.1450377; //convert kpa to PSI  
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

void dispClock (Adafruit_SSD1306 *display){
    byte hourAdj;
    display->clearDisplay();             //clear buffer
    if (clockOffset + hour > 23) {        // ensure hours don't exceed 23
      hourAdj = clockOffset + hour - 24;
    }
    else {
      hourAdj = clockOffset + hour;
    }

    byte nDig = digits(hourAdj)+3;
    byte center = 63;
    
    display->setTextColor(WHITE);
    display->setTextSize(3);             // text size
    display->setCursor(center-((nDig*18)/2),6);
    display->print(hourAdj); 
    display->print(':');
    if (minute < 10) { display->print('0'); } //keep time format for minutes
    display->println(minute);
    display->display();
}

// This function helps when centering text. Number of display digits are returned
byte digits(float val){
  byte nDigits;
  if (val >= 0){ 
    if (val < 10)         {nDigits = 1;}
    else if (val < 100)   {nDigits = 2;}
    else if (val < 1000)  {nDigits = 3;}
    else if (val < 10000) {nDigits = 4;}
  }
  else {
    if (val > -10)        {nDigits = 2;}
    else if (val > -100)  {nDigits = 3;}
    else if (val > -1000) {nDigits = 4;}
  }
  return nDigits;
}