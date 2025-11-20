/*
 * ========================================
 * DISPLAY FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "display.h"
#include "globals.h"
#include "image_data.h"
#include "menu.h"
#include <EEPROM.h>

void dispMenu() {
  switch (dispArray1[0]) {  // Level 0 - Main menu selection
    
    case 1:  // Oil Pressure Display                 dispArray1 = {1, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag (no action - this is a display screen only)
      }
      // Serial.println("Oil Pressure");  // Debug output
      dispOilPrsGfx(&display1);  // Show oil pressure with icon
      break;
    
    case 2:  // Coolant Temperature Display           dispArray1 = {2, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Coolant Temp");  // Debug output
      dispCoolantTempGfx(&display1);  // Show coolant temp with thermometer icon
      break;
    
    case 3:  // Oil Temperature Display                dispArray1 = {3, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag   
      }
      // Serial.println("Oil Temp");  // Debug output
      dispOilTempGfx(&display1);  // Show oil temp with oil can/thermometer icon
      break;
    
    case 4:  // Fuel Level Display                     dispArray1 = {4, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Fuel Level");  // Debug output
      dispFuelLvlGfx(&display1);  // Show fuel level in gallons/liters with icon
      break;
    
    case 5:  // Battery Voltage Display                dispArray1 = {5, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("batt voltage");  // Debug output
      dispBattVoltGfx(&display1);  // Show battery voltage with battery icon
      break;

    case 6:  // Clock Display                          dispArray1 = {6, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Clock");  // Debug output
      dispClock(&display1);  // Show time from GPS with local offset
      break;  

    case 7:  // Trip Odometer with Reset Option        dispArray1 = {7, x, x, x}
	  if (menuLevel == 0 && button == 1) {
        // Button pressed - enter submenu to confirm reset
        button = 0;
        menuLevel = 1;   // Go to level 1 (submenu)
        nMenuLevel = 1;  // 2 options in submenu: Yes/No (0-indexed)
      } 
      else if (menuLevel == 0) {
        // No button - just display trip odometer value
        dispTripOdo(&display1);
        // Serial.println("Trip Odo");  // Debug output
      } 
      else {
        // In submenu - handle Yes/No selection for reset
        switch (dispArray1[1]) {
          case 0:  // Reset Trip Odo: YES              dispArray1 = {7, 0, x, x}
            dispOdoResetYes(&display1);  // Show confirmation screen with YES highlighted
			      if (button == 1) {
                // User confirmed reset
			        odoTrip = 0;  // Clear trip odometer
			        goToLevel0();  // Return to main menu
			        dispArray1[0] = 7;  // Stay on trip odo screen
            } 
            break;
            
		      case 1:  // Reset Trip Odo: NO               dispArray1 = {7, 1, x, x}
			      dispOdoResetNo(&display1);  // Show confirmation screen with NO highlighted
			      if (button == 1) {
                // User cancelled reset
				      goToLevel0();  // Return to main menu
				      dispArray1[0] = 7;  // Stay on trip odo screen
			      } 
          break;
		    } 
	    }
      break;    
    
    case 8:  // Speed Display                          dispArray1 = {8, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Speed");  // Debug output
      dispSpd(&display1);  // Show speed in km/h or mph based on units setting
      break;  
    
    case 9:  // RPM Display                            dispArray1 = {9, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("RPM");  // Debug output
      dispRPM(&display1);  // Show engine RPM
      break;  
    
    case 10:  // Ignition Timing Display               dispArray1 = {10, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("ignition timing");  // Debug output
      dispIgnAng(&display1);  // Show ignition advance in degrees BTDC
      break;
    
    case 11:  // Air/Fuel Ratio Display                dispArray1 = {11, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("AFR");  // Debug output
      dispAFR(&display1);  // Show AFR (e.g., 14.7)
      break;  
    
    case 12:  // Fuel Pressure Display                 dispArray1 = {12, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Fuel Pressure");  // Debug output
      dispFuelPrs(&display1);  // Show fuel pressure in PSI or bar
      break;  

    case 13:  // Fuel Composition Display              dispArray1 = {13, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("ethanol %");  // Debug output
      dispFuelComp(&display1);  // Show ethanol percentage (E85 flex fuel)
      break;  

    case 14:  // Injector Duty Cycle Display           dispArray1 = {14, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Inj Duty");  // Debug output
      dispInjDuty(&display1);  // Show injector duty cycle percentage
      break;
      
    case 15:  // Falcon Script Logo                    dispArray1 = {15, x, x, x}
      if (menuLevel == 0 && button == 1) {
        // No action on button press - just a splash screen
        //goToLevel0();
      }
      // Serial.println("falcon Script");  // Debug output
      dispFalconScript(&display1);  // Display Falcon logo
      break;  

    case 0:  // SETTINGS MENU - Always last screen for easy wrapping access
             // This is a complex multi-level menu for system configuration
             // Structure: Settings -> [Display 2 Select | Units | Clock Offset | Fuel Sensor Cal]
             //   -> Display 2: 9 screen options (Oil Prs, Coolant, Batt, Fuel, AFR, Speed, RPM, 302V, 302CID)
             //   -> Units: Metric or Imperial
             //   -> Clock Offset: -12 to +12 hours from UTC
             //   -> Fuel Cal: Adjust fuel sender calibration
             
      if (menuLevel == 0 && button == 1) {  
        // Button pressed - enter Settings submenu
        button = 0;
        menuLevel = 1;   // Go to level 1 (submenu)
        nMenuLevel = 3;  // 4 options: Display2, Units, Clock, FuelCal (0-indexed)
      } 
      else if (menuLevel == 0) {
        // No button - display "SETTINGS" screen
        // Serial.println("settings");  // Debug output
        dispSettings(&display1);
      } 
      else {  
        // In Settings submenu - handle level 1, 2, and 3 navigation
        
        switch (dispArray1[1]) {  // Level 1 selection
          
          case 0:  // Configure Display 2           dispArray1 = {0, 0, x, x}
            if (menuLevel == 1 && button == 1) {
              // Enter Display 2 selection submenu
              button = 0;
              menuLevel = 2;   // Go to level 2
              nMenuLevel = 8;  // 9 display options (0-indexed)
            } 
            else if (menuLevel == 1) {
              // Show "DISPLAY 2" menu header
              //Serial.println("Display 2");  // Debug output
              dispDisp2Select(&display1);
            } 
            else {
              // Level 2 - Select what to show on Display 2
              switch (dispArray1[2]) {
                
                case 0:  // Oil Pressure on Display 2    dispArray1 = {0, 0, 0, x}
                  //Serial.println("Disp2: Oil Pressure");  // Debug output
                  dispArray2[0] = 0;  // Set display 2 to oil pressure
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 1:  // Coolant Temp on Display 2    dispArray1 = {0, 0, 1, x}
                  //Serial.println("Disp2: Coolant Temp");  // Debug output
                  dispArray2[0] = 1;  // Set display 2 to coolant temp
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 2:  // Battery Voltage on Display 2  dispArray1 = {0, 0, 2, x}
                  //Serial.println("Disp2: Battery Voltage");  // Debug output
                  dispArray2[0] = 2;  // Set display 2 to battery voltage
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 3:  // Fuel Level on Display 2       dispArray1 = {0, 0, 3, x}
                  //Serial.println("Disp2: Fuel Level");  // Debug output
                  dispArray2[0] = 3;  // Set display 2 to fuel level
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu;
                  }
                  break;
                  
                case 4:  // RPM on Display 2              dispArray1 = {0, 0, 4, x}
                  //Serial.println("Disp2: RPM");  // Debug output
                  dispArray2[0] = 4;  // Set display 2 to RPM
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 5:  // Speed on Display 2            dispArray1 = {0, 0, 5, x}
                  //Serial.println("Disp2: Speed");  // Debug output
                  dispArray2[0] = 5;  // Set display 2 to speed
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;

                case 6:  // Clock on Display 2            dispArray1 = {0, 0, 6, x}
                  //Serial.println("Clock");  // Debug output
                  dispArray2[0] = 6;  // Set display 2 to clock
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;

                case 7:  // 302CID Logo on Display 2      dispArray1 = {0, 0, 7, x}
                  //Serial.println("Disp2: 302CID");  // Debug output
                  dispArray2[0] = 7;  // Set display 2 to 302 CID logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 8:  // 302V Logo on Display 2        dispArray1 = {0, 0, 8, x}
                  //Serial.println("Disp2: 302V");  // Debug output
                  dispArray2[0] = 8;  // Set display 2 to 302V logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 9:  // Falcon Script on Display 2    dispArray1 = {0, 0, 9, x}
                  //Serial.println("Disp2: Falcon Script");  // Debug output
                  dispArray2[0] = 9;  // Set display 2 to Falcon logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                
              } // End switch dispArray1[2] - Display 2 options
            } // End level 2 - Display 2 selection
            break;  // End case 0 - Display 2 submenu
 
          case 1:  // Configure Units (Metric/Imperial)  dispArray1 = {0, 1, x, x}
            if (menuLevel == 1 && button == 1) {
              // Enter Units selection submenu
              button = 0;
              menuLevel = 2;   // Go to level 2
              nMenuLevel = 1;  // 2 options: Metric or Imperial (0-indexed)
            } 
            else if (menuLevel == 1) {
              // Show "UNITS" menu header
              //Serial.println("Units");  // Debug output
              dispUnits(&display1);
            } 
            else {
              // Level 2 - Select metric or imperial units
              switch (dispArray1[2]) {
                
                case 0:  // Metric Units (km/h, C, bar)  dispArray1 = {0, 1, 0, x}
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();
                  display1.setTextSize(2);
                  display1.setCursor(31,8);
                  display1.println("Metric");  // Display selected unit system
                  display1.display();
                  units = 0;  // Set to metric
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 1:  // Imperial Units (mph, F, PSI)  dispArray1 = {0, 1, 1, x}
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();
                  display1.setTextSize(2);
                  display1.setCursor(20,8);
                  display1.println("'Merican");  // Display selected unit system
                  display1.display();
                  units = 1;  // Set to imperial
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
              } // End switch dispArray1[2] - Units options
            } // End level 2 - Units selection
            break;  // End case 1 - Units submenu

          case 2:  // Clock Offset (Time Zone)          dispArray1 = {0, 2, x, x}
            // Adjust clock offset from UTC (-12 to +12 hours)
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;  // Go to level 2 (offset value selection)
              // nMenuLevel set dynamically in level 2 handler
            } 
            else if (menuLevel == 1) {
              // Show "SET CLOCK" menu header
              // Serial.println("ClockOffset");  // Debug output
              dispClockOffset(&display1);
            } 
            else {
              // Level 2 - Display current offset and adjust with encoder
              if (button == 1) {
                // Button pressed - save clock offset and return to main menu
                detachInterrupt(0);  // Temporarily detach encoder interrupts
                detachInterrupt(1);
                attachInterrupt(0, rotate, CHANGE);  // Reattach normal menu rotation handler
                attachInterrupt(1, rotate, CHANGE);
                EEPROM.write(clockOffset, clockOffsetAddress);  // Save offset to EEPROM
                goToLevel0();  // Return to main menu
              } 
              else {
                // Rotary encoder adjusts clock offset value (-12 to +12 hours)
                // Temporarily change encoder handler to modify clockOffset directly
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, incrementOffset, CHANGE);  // Use special offset increment handler
                attachInterrupt(1, incrementOffset, CHANGE);
                dispClock(&display1);  // Display time with current offset applied
              }
            } // End level 2 - Clock offset adjustment
            break;  // End case 2 - Clock offset submenu
          
          case 3:  // Exit Settings Menu                dispArray1 = {0, 3, x, x}
            display1.setTextColor(WHITE); 
            display1.clearDisplay();
            display1.setTextSize(2);
            display1.setCursor(35,8);
            display1.println("EXIT");  // Display "EXIT" text               
            display1.display();
            if (button == 1) {
              goToLevel0();  // Return to main menu
            }
            break;  // End case 3 - Exit
            
        } // End switch dispArray1[1] - Settings level 1 options
        break;  // Break for case 0 (Settings menu when in submenus)
      } // End Settings submenu levels 1-3
  } // End switch dispArray1[0] - Main menu selection
} // End dispMenu()

/**
 * disp2 - Control display 2 based on saved selection
 * 
 * Routes display 2 output based on dispArray2[0], which is set in Settings menu
 * and saved to EEPROM. Allows user to customize what appears on the second display.
 * 
 * Display options:
 * 0 - Oil Pressure
 * 1 - Coolant Temperature
 * 2 - Battery Voltage
 * 3 - Fuel Level
 * 4 - RPM
 * 5 - Speed
 * 6 - Clock
 * 7 - 302CID logo
 * 8 - 302V logo
 * 9 - Falcon Script logo
 */
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
    
    case 7: // 302CID Logo
      disp302CID(&display2);
      break;

    case 8: // 302V Logo
      disp302V(&display2);
      break;

    case 9: // Falcon Script Logo
      dispFalconScript(&display2);
      break;
  }
}

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
    display->drawBitmap(0, 0, IMG_OIL_TEMP, 40, 32, 1);  // Draw oil/temp icon (40x32 pixels)
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
    display->drawBitmap(0, 0, IMG_FALCON_SCRIPT, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302CID - Display 302 CID engine badge
 * Shows "302 CID" (Cubic Inch Displacement) logo
 */
void disp302CID(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, IMG_302_CID, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302V - Display 302 V8 engine badge
 * Shows "302V" (V8) logo with graphic
 */
void disp302V(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, IMG_302V, SCREEN_W, SCREEN_H, 1);
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
    display->drawBitmap(0, 0, IMG_OIL_PRS, 40, 32, 1);  // Draw oil can icon
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
    display->drawBitmap(0, 0, IMG_OIL_TEMP, 40, 32, 1);
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
    display->drawBitmap(0, 0, IMG_COOLANT_TEMP, 38, 32, 1);
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
    display->drawBitmap(0, 0, IMG_BATT_VOLT, 35, 32, 1);
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
    display->drawBitmap(0, 0, IMG_FUEL_LVL, 32, 32, 1);
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
