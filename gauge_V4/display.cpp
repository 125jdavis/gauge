/*
 * ========================================
 * DISPLAY FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "display.h"
#include "globals.h"
#include "image_data.h"
#include "menu.h"
#include "utilities.h"
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

    case 16:  // Boost Gauge Display (with bar)        dispArray1 = {16, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Boost GFX");  // Debug output
      dispBoostGfx(&display1);  // Show boost with turbo icon and bar gauge
      break;

    case 17:  // Boost Display (text only)             dispArray1 = {17, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Boost Text");  // Debug output
      dispBoost(&display1);  // Show boost with turbo icon, text only
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
              nMenuLevel = 11;  // 12 display options (0-indexed): 0-11 now includes both boost options
              // Validate dispArray1[2] is in valid range (prevent EEPROM corruption issues)
              if (dispArray1[2] > 11) {
                dispArray1[2] = 0;  // Reset to first option if out of range
              }
              // Force mode change detection so display updates immediately
              dispArray1_prev[0] = 255;  // Set to invalid value to force update
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
                  
                case 10:  // Boost Gauge on Display 2 (with bar)  dispArray1 = {0, 0, 10, x}
                  //Serial.println("Disp2: Boost GFX");  // Debug output
                  dispArray2[0] = 10;  // Set display 2 to boost gauge with bar
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;

                case 11:  // Boost Display on Display 2 (text)  dispArray1 = {0, 0, 11, x}
                  //Serial.println("Disp2: Boost Text");  // Debug output
                  dispArray2[0] = 11;  // Set display 2 to boost text display
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
              // Validate dispArray1[2] is in valid range (prevent EEPROM corruption issues)
              if (dispArray1[2] > 1) {
                dispArray1[2] = 0;  // Reset to first option if out of range
              }
              // Force mode change detection so display updates immediately
              dispArray1_prev[0] = 255;  // Set to invalid value to force update
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
              // Force mode change detection so dispClock updates immediately
              dispArray1_prev[0] = 255;  // Set to invalid value to force update
              // Switch encoder handlers to clock offset adjustment mode
              detachInterrupt(0);
              detachInterrupt(1);
              attachInterrupt(0, incrementOffset, CHANGE);  // Use special offset increment handler
              attachInterrupt(1, incrementOffset, CHANGE);
              // nMenuLevel set dynamically in level 2 handler
            } 
            else if (menuLevel == 1) {
              // Show "SET CLOCK" menu header at level 1
              // Serial.println("ClockOffset");  // Debug output
              dispClockOffset(&display1);
            } 
            else {
              // Level 2 - Display clock while adjusting offset with encoder
              // Always show the clock so user can see the time as they adjust
              if (button == 1) {
                // Button pressed - save clock offset and return to main menu
                button = 0;  // Clear button flag immediately
                detachInterrupt(0);  // Detach offset adjustment handlers
                detachInterrupt(1);
                attachInterrupt(0, rotate, CHANGE);  // Reattach normal menu rotation handler
                attachInterrupt(1, rotate, CHANGE);
                EEPROM.write(clockOffsetAddress, clockOffset);  // Save offset to EEPROM (address, value)
                goToLevel0();  // Return to main menu
              } 
              else {
                // Display clock with current offset applied
                // Encoder rotation handled by incrementOffset ISR
                dispClock(&display1);
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
  
  // Update previous display mode for dirty tracking
  for (int i = 0; i < 4; i++) {
    dispArray1_prev[i] = dispArray1[i];
  }
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
 * 10 - Boost Gauge (with bar)
 * 11 - Boost Display (text only)
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

    case 10: // Boost Gauge (with bar)
      dispBoostGfx(&display2);  // Show boost with turbo icon and bar gauge
      break;

    case 11: // Boost Display (text only)
      dispBoost(&display2);  // Show boost with turbo icon, text only
      break;
  }
  
  // Update previous display mode for dirty tracking
  dispArray2_prev = dispArray2[0];
}

void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(16,8);  // Centered for "SETTINGS" (8 chars * 12px = 96px, (128-96)/2 = 16)
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
    display->setCursor(10,8);  // Centered for "DISPLAY 2" (9 chars * 12px = 108px, (128-108)/2 = 10)
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
    display->setCursor(34,8);  // Centered for "UNITS" (5 chars * 12px = 60px, (128-60)/2 = 34)
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
    display->setCursor(10,9);  // Centered for "SET CLOCK" (9 chars * 12px = 108px, (128-108)/2 = 10)
    display->println("SET CLOCK");                 
    display->display();
}

void dispRPM (Adafruit_SSD1306 *display){
    // Check if mode changed or RPM changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_RPM(RPM, RPM_prev)) {
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
      
      // Update previous value
      RPM_prev = RPM;
    }
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
    // Check if mode changed or speed changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Speed(spd, spd_prev)) {
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
      
      // Update previous value
      spd_prev = spd;
    }
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
    // Check if mode changed or temperature changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Temperature(oilTemp, oilTemp_prev)) {
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
      
      // Update previous value
      oilTemp_prev = oilTemp;
    }
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
    // Check if mode changed or fuel pressure changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Pressure(fuelPrs, fuelPrs_prev, units)) {
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
      
      // Update previous value
      fuelPrs_prev = fuelPrs;
    }
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
    // Check if mode changed or fuel composition changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 1% change in ethanol content
    if (modeChanged || abs(fuelComp - fuelComp_prev) > 1.0) {
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
      
      // Update previous value
      fuelComp_prev = fuelComp;
    }
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
    // Check if mode changed or AFR changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // AFR uses same threshold as temperature (>1.0 change)
    if (modeChanged || abs(afr - afr_prev) > 0.1) {  // 0.1 AFR change threshold
      display->setTextColor(WHITE); 
      display->clearDisplay();
      display->setCursor(8,6);
      display->setTextSize(3); 
      display->print(afr, 1);  // Print AFR with 1 decimal place (e.g., 14.7)
      display->setCursor(88,10);
      display->setTextSize(2);
      display->println("AFR");         
      display->display();
      
      // Update previous value
      afr_prev = afr;
    }
}

/**
 * dispFalconScript - Display Falcon logo splash screen
 * Simple bitmap display - shows Falcon script logo
 * Optimized: Only draws once, then skips updates for static content
 */
void dispFalconScript(Adafruit_SSD1306 *display) {
    // Check if display mode changed (need to redraw)
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
      if (modeChanged) {
        staticContentDrawn1 = false;
      }
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
      if (modeChanged) {
        staticContentDrawn2 = false;
      }
    }
    
    // Only draw if not already drawn (static content optimization)
    if ((display == &display1 && !staticContentDrawn1) || 
        (display == &display2 && !staticContentDrawn2)) {
      display->clearDisplay();
      display->drawBitmap(0, 0, IMG_FALCON_SCRIPT, SCREEN_W, SCREEN_H, 1);
      display->display();
      
      // Mark static content as drawn
      if (display == &display1) {
        staticContentDrawn1 = true;
      } else {
        staticContentDrawn2 = true;
      }
    }
}

/**
 * disp302CID - Display 302 CID engine badge
 * Shows "302 CID" (Cubic Inch Displacement) logo
 * Optimized: Only draws once, then skips updates for static content
 */
void disp302CID(Adafruit_SSD1306 *display) {
    // Check if display mode changed (need to redraw)
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
      if (modeChanged) {
        staticContentDrawn1 = false;
      }
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
      if (modeChanged) {
        staticContentDrawn2 = false;
      }
    }
    
    // Only draw if not already drawn (static content optimization)
    if ((display == &display1 && !staticContentDrawn1) || 
        (display == &display2 && !staticContentDrawn2)) {
      display->clearDisplay();
      display->drawBitmap(0, 0, IMG_302_CID, SCREEN_W, SCREEN_H, 1);
      display->display();
      
      // Mark static content as drawn
      if (display == &display1) {
        staticContentDrawn1 = true;
      } else {
        staticContentDrawn2 = true;
      }
    }
}

/**
 * disp302V - Display 302 V8 engine badge
 * Shows "302V" (V8) logo with graphic
 * Optimized: Only draws once, then skips updates for static content
 */
void disp302V(Adafruit_SSD1306 *display) {
    // Check if display mode changed (need to redraw)
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
      if (modeChanged) {
        staticContentDrawn1 = false;
      }
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
      if (modeChanged) {
        staticContentDrawn2 = false;
      }
    }
    
    // Only draw if not already drawn (static content optimization)
    if ((display == &display1 && !staticContentDrawn1) || 
        (display == &display2 && !staticContentDrawn2)) {
      display->clearDisplay();
      display->drawBitmap(0, 0, IMG_302V, SCREEN_W, SCREEN_H, 1);
      display->display();
      
      // Mark static content as drawn
      if (display == &display1) {
        staticContentDrawn1 = true;
      } else {
        staticContentDrawn2 = true;
      }
    }
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
    // Check if mode changed or pressure changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Pressure(oilPrs, oilPrs_prev, units)) {
      float oilPrsDisp;
      display->setTextColor(WHITE); 
      display->clearDisplay();
      display->drawBitmap(0, 0, IMG_OIL_PRS, 40, 32, 1);  // Draw oil can icon
      
      if (units == 0){    // Metric Units (bar)
        oilPrsDisp = (oilPrs < 0) ? 0 : oilPrs/100;  // Convert kPa to bar, clamp negative
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
        oilPrsDisp = (oilPrs < 0) ? 0 : oilPrs * 0.1450377;  // Convert kPa to PSI, clamp negative
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
      
      // Update previous value
      oilPrs_prev = oilPrs;
    }
}

void dispOilTempGfx (Adafruit_SSD1306 *display) {
    // Check if mode changed or temperature changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Temperature(oilTemp, oilTemp_prev)) {
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
      
      // Update previous value
      oilTemp_prev = oilTemp;
    }
}

void dispCoolantTempGfx (Adafruit_SSD1306 *display) {
    // Check if mode changed or temperature changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Only update if mode changed or value changed significantly
    if (modeChanged || needsUpdate_Temperature(coolantTemp, coolantTemp_prev)) {
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
      
      // Update previous value
      coolantTemp_prev = coolantTemp;
    }
}

void dispBattVoltGfx (Adafruit_SSD1306 *display) {
    // Check if mode changed or battery voltage changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 0.1V change
    if (modeChanged || abs(vBatt - vBatt_prev) > 0.1) {
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
      
      // Update previous value
      vBatt_prev = vBatt;
    }
}

void dispFuelLvlGfx (Adafruit_SSD1306 *display) {
    // Check if mode changed or fuel level changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 0.5 gallons/liters change
    if (modeChanged || abs(fuelLvl - fuelLvl_prev) > 0.5) {
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
      
      // Update previous value
      fuelLvl_prev = fuelLvl;
    }
}

void dispTripOdo (Adafruit_SSD1306 *display) {
    // Check if mode changed or odometer changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 0.1 km/miles change
    if (modeChanged || abs(odoTrip - odoTrip_prev) > 0.1) {
      float odoDisp;
      display->setTextColor(WHITE); 
      display->clearDisplay();             //clear buffer
          
      if (units == 0){    // Metric Units
        odoDisp = odoTrip; 
        display->setCursor(100,10);
        display->setTextSize(2);
        display->println("km");         
      } 
      else {              // 'Merican units
        odoDisp = odoTrip * 0.6213712; //convert km to miles  
        display->setCursor(100,10);
        display->setTextSize(2);
        display->println("mi");          
      }

      display->setCursor(35,10);
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
      display->setCursor(1,7);
      display->println("Trip");
      display->setCursor(1,17);
      display->println("Odo:"); 
      display->display();
      
      // Update previous value
      odoTrip_prev = odoTrip;
    }
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
    // Check if mode changed or ignition angle changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 10 units (1 degree, since stored as degrees * 10)
    if (modeChanged || abs(ignAngCAN - ignAngCAN_prev) > 10) {
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
      
      // Update previous value
      ignAngCAN_prev = ignAngCAN;
    }
}

void dispInjDuty (Adafruit_SSD1306 *display) {
    // Check if mode changed or injector duty changed enough to warrant update
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Threshold: 10 units (1%, since stored as % * 10)
    if (modeChanged || abs(injDutyCAN - injDutyCAN_prev) > 10) {
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
      
      // Update previous value
      injDutyCAN_prev = injDutyCAN;
    }
}


/**
 * dispBoostGfx - Display boost pressure with turbo icon and bar gauge
 * 
 * Shows boost/manifold pressure with turbo icon on the left and horizontal bar gauge.
 * Bar gauge displays pressure range with visual representation.
 * Supports both metric (kPa) and imperial (PSI) units.
 * 
 * @param display - Pointer to display object
 */
void dispBoostGfx(Adafruit_SSD1306 *display) {
  // Check if mode changed or boost pressure changed enough to warrant update
  bool modeChanged = false;
  if (display == &display1) {
    modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
  } else {
    modeChanged = (dispArray2[0] != dispArray2_prev);
  }
  
  // Only update if mode changed or value changed significantly
  if (modeChanged || needsUpdate_Boost(boostPrs, boostPrs_prev, units)) {
    // Bar gauge constants
    const int BAR_X = 29;
    const int BAR_Y = 8;
    const int BAR_WIDTH = 96;
    const int BAR_HEIGHT = 18;
    
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // Draw turbo icon on the left
    display->drawBitmap(0, 4, IMG_TURBO, 24, 30, 1);
    
    if (units == 0) {  // Metric units (kPa)
      float kpa = boostPrs;
      const float BAR_MIN = 0.0;
      const float BAR_MAX = 300.0;
      const float ZERO_KPA = 101.0;  // Atmospheric pressure
      
      // Draw 2px bar outline with rounded corners
      display->drawRect(BAR_X - 2, BAR_Y - 2, BAR_WIDTH + 4, BAR_HEIGHT + 4, SSD1306_WHITE);
      display->drawRect(BAR_X - 1, BAR_Y - 1, BAR_WIDTH + 2, BAR_HEIGHT + 2, SSD1306_WHITE);
      
      // Clear corner pixels for rounded appearance
      display->drawPixel(BAR_X - 2, BAR_Y - 2, 0);
      display->drawPixel(BAR_X + BAR_WIDTH + 1, BAR_Y - 2, 0);
      display->drawPixel(BAR_X + BAR_WIDTH + 1, BAR_Y + BAR_HEIGHT + 1, 0);
      display->drawPixel(BAR_X - 2, BAR_Y + BAR_HEIGHT + 1, 0);
      
      int innerY = BAR_Y + 1;
      int innerH = BAR_HEIGHT - 2;
      
      // Calculate bar position and zero point
      float barPosition = mapFloat(kpa, BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
      barPosition = constrain(barPosition, 0, BAR_WIDTH);
      float zeroPosition = mapFloat(ZERO_KPA, BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
      
      int barPos = BAR_X + barPosition;
      int zeroX = BAR_X + zeroPosition;
      
      // Fill bar: checkered for vacuum, solid for boost
      if (kpa >= ZERO_KPA) {
        // Boost - solid fill
        int fillW = barPos - zeroX;
        if (fillW > 0) {
          display->fillRect(zeroX, innerY, fillW, innerH, SSD1306_WHITE);
        }
      } else {
        // Vacuum - checkered pattern
        int fillX = barPos;
        int fillW = zeroX - barPos;
        
        for (int x = fillX; x < zeroX; x++) {
          for (int y = innerY; y < innerY + innerH; y++) {
            if ((((zeroX - x) >> 1) + ((y - innerY) >> 1)) & 1) {
              display->drawPixel(x, y, SSD1306_WHITE);
            }
          }
        }
      }
      
      // Draw tick marks at key pressure points
      float ticks[] = { 50, 100, 150, 200, 250 };
      for (byte i = 0; i < 5; i++) {
        float px = mapFloat(ticks[i], BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
        int x = BAR_X + px;
        display->drawFastVLine(x, innerY, innerH, SSD1306_WHITE);
      }
      
    } else {  // Imperial units (PSI)
      // Convert to gauge pressure (relative to atmospheric)
      float psi = boostPrs * 0.1450377 - 14.7;
      const float BAR_MIN = -14.7;
      const float BAR_MAX = 29.4;
      
      // Draw 2px bar outline with rounded corners
      display->drawRect(BAR_X - 2, BAR_Y - 2, BAR_WIDTH + 4, BAR_HEIGHT + 4, SSD1306_WHITE);
      display->drawRect(BAR_X - 1, BAR_Y - 1, BAR_WIDTH + 2, BAR_HEIGHT + 2, SSD1306_WHITE);
      
      // Clear corner pixels for rounded appearance
      display->drawPixel(BAR_X - 2, BAR_Y - 2, 0);
      display->drawPixel(BAR_X + BAR_WIDTH + 1, BAR_Y - 2, 0);
      display->drawPixel(BAR_X + BAR_WIDTH + 1, BAR_Y + BAR_HEIGHT + 1, 0);
      display->drawPixel(BAR_X - 2, BAR_Y + BAR_HEIGHT + 1, 0);
      
      int innerY = BAR_Y + 1;
      int innerH = BAR_HEIGHT - 2;
      
      // Calculate bar and zero positions
      float barPosition = mapFloat(psi, BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
      barPosition = constrain(barPosition, 0, BAR_WIDTH);
      float zeroPosition = mapFloat(0, BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
      
      int barPos = BAR_X + barPosition;
      int zeroX = BAR_X + zeroPosition;
      
      // Fill bar based on positive (boost) or negative (vacuum) pressure
      if (psi >= 0) {
        // Positive boost - solid fill
        int fillW = barPos - zeroX;
        if (fillW > 0) {
          display->fillRect(zeroX, innerY, fillW, innerH, SSD1306_WHITE);
        }
      } else {
        // Negative pressure (vacuum) - checkered pattern
        int fillX = barPos;
        int fillW = zeroX - barPos;
        
        for (int x = fillX; x < zeroX; x++) {
          for (int y = innerY; y < innerY + innerH; y++) {
            if ((((zeroX - x) >> 1) + ((y - innerY) >> 1)) & 1) {
              display->drawPixel(x, y, SSD1306_WHITE);
            }
          }
        }
      }
      
      // Draw tick marks at key pressure points
      float ticks[] = { -7.3, 0, 7.3, 14.7, 21.8 };
      for (byte i = 0; i < 5; i++) {
        float px = mapFloat(ticks[i], BAR_MIN, BAR_MAX, 0, BAR_WIDTH);
        int x = BAR_X + px;
        display->drawFastVLine(x, innerY, innerH, SSD1306_WHITE);
      }
    }
    
    display->display();
    
    // Update previous value
    boostPrs_prev = boostPrs;
  }
}

/**
 * dispBoost - Display boost pressure with turbo icon (text only, no bar)
 * 
 * Shows boost/manifold pressure with turbo icon on the left.
 * Text-only display with right-aligned values and units.
 * Supports both metric (kPa) and imperial (PSI) units.
 * 
 * @param display - Pointer to display object
 */
void dispBoost(Adafruit_SSD1306 *display) {
  // Check if mode changed or boost pressure changed enough to warrant update
  bool modeChanged = false;
  if (display == &display1) {
    modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
  } else {
    modeChanged = (dispArray2[0] != dispArray2_prev);
  }
  
  // Only update if mode changed or value changed significantly
  if (modeChanged || needsUpdate_Boost(boostPrs, boostPrs_prev, units)) {
    display->setTextColor(WHITE);
    display->clearDisplay();
    
    // Draw turbo icon on the left side
    display->drawBitmap(0, 4, IMG_TURBO, 24, 30, 1);
    
    if (units == 0) {  // Metric units (kPa)
      float kpa = boostPrs;
      int kpaInt = (int)kpa;
      
      // Position units label at right side (text size 2)
      // Display width is 128px, "kPa" is 3 chars * 12px = 36px
      // Position at 92 so text ends at 128 (92 + 36 = 128)
      display->setTextSize(2);
      display->setCursor(92, 10);  // Fixed position for units on right
      display->print("kPa");
      
      // Calculate position for value (right-aligned next to units)
      // Text size 2: each digit is 12px wide
      display->setTextSize(3);
      byte nDig = digits(kpaInt);
      int valueX = 92 - (nDig * 18) - 3;  // 3px gap before units
      display->setCursor(valueX, 6);
      display->print(kpaInt, DEC);
      
    } else {  // Imperial units (PSI)
      // Convert to gauge pressure (relative to atmospheric)
      float psi = boostPrs * 0.1450377 - 14.7;
      
      // Position units label at right side (text size 2)
      // Display width is 128px, "PSI" is 3 chars * 12px = 36px
      // Position at 92 so text ends at 128 (92 + 36 = 128)
      // DUE TO SPACE LIMITATIONS, UNIT IS OMITTED
      // display->setTextSize(1);
      // display->setCursor(102, 10);  // Fixed position for units on right
      // display->print("PSI");
      
      // Calculate position for value with 1 decimal (right-aligned next to units)
      // Text size 2: each digit/decimal is ~12px wide, decimal point ~6px
      display->setTextSize(3);
      byte nDig = digits(psi);
      // Account for decimal point and one decimal digit: add ~18px (1 digit + point)
      int valueX = 78 - (nDig* 18);    
      display->setCursor(valueX, 6);
      display->print(psi, 1);
    }
    
    display->display();
    
    // Update previous value
    boostPrs_prev = boostPrs;
  }
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
    // Check if mode changed or time changed or clockOffset changed
    bool modeChanged = false;
    if (display == &display1) {
      modeChanged = needsUpdate_ModeChange(dispArray1, dispArray1_prev, 4);
    } else {
      modeChanged = (dispArray2[0] != dispArray2_prev);
    }
    
    // Check if clock offset changed (for adjustment mode)
    bool offsetChanged = (clockOffset != clockOffset_prev);
    
    // Only update if mode changed or time changed or offset changed
    if (modeChanged || needsUpdate_Time(hour, minute, hour_prev, minute_prev) || offsetChanged) {
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
      
      // Update previous values
      hour_prev = hour;
      minute_prev = minute;
      clockOffset_prev = clockOffset;  // Track previous offset
    }
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
 * DISPLAY OPTIMIZATION FUNCTIONS
 * ========================================
 * These functions implement dirty tracking and variable refresh rates
 * to minimize unnecessary display updates and improve performance.
 */

/**
 * needsUpdate_Temperature - Check if temperature changed enough to warrant update
 * 
 * Temperature must change more than 1 degree (F or C) to trigger update
 * 
 * @param current - Current temperature value
 * @param previous - Previous temperature value
 * @return true if update needed, false otherwise
 */
bool needsUpdate_Temperature(float current, float previous) {
  return (abs(current - previous) > 1.0);
}

/**
 * needsUpdate_Pressure - Check if pressure changed enough to warrant update
 * 
 * Pressure change must be:
 * - > 10 kPa (metric units), or
 * - > 1 PSI (imperial units, ~6.89 kPa)
 * 
 * @param current - Current pressure in kPa
 * @param previous - Previous pressure in kPa
 * @param units - 0=metric, 1=imperial
 * @return true if update needed, false otherwise
 */
bool needsUpdate_Pressure(float current, float previous, byte units) {
  float threshold = (units == 0) ? 10.0 : 6.89;  // 10 kPa or 1 PSI
  return (abs(current - previous) > threshold);
}

/**
 * needsUpdate_Speed - Check if speed changed enough to warrant update
 * 
 * Speed change must be > 1 mph or km/h
 * 
 * @param current - Current speed (km/h * 100)
 * @param previous - Previous speed (km/h * 100)
 * @return true if update needed, false otherwise
 */
bool needsUpdate_Speed(int current, int previous) {
  return (abs(current - previous) > 100);  // >1 km/h (stored as km/h * 100)
}

/**
 * needsUpdate_RPM - Check if RPM changed enough to warrant update
 * 
 * RPM change must be > 20 rpm
 * 
 * @param current - Current RPM
 * @param previous - Previous RPM
 * @return true if update needed, false otherwise
 */
bool needsUpdate_RPM(int current, int previous) {
  return (abs(current - previous) > 20);
}

/**
 * needsUpdate_Boost - Check if boost pressure changed enough to warrant update
 * 
 * Boost pressure change must be:
 * - > 2 kPa (metric units), or
 * - > 0.3 PSI (imperial units, ~2.07 kPa)
 * 
 * @param current - Current boost/MAP pressure in kPa
 * @param previous - Previous boost/MAP pressure in kPa
 * @param units - 0=metric, 1=imperial
 * @return true if update needed, false otherwise
 */
bool needsUpdate_Boost(float current, float previous, byte units) {
  float threshold = (units == 0) ? 2.0 : 2.07;  // 2 kPa or 0.3 PSI
  return (abs(current - previous) > threshold);
}

/**
 * needsUpdate_Time - Check if time display should update
 * 
 * Updates when minute changes (hour changes are implicit)
 * 
 * @param hour_curr - Current hour
 * @param minute_curr - Current minute
 * @param hour_prev - Previous hour
 * @param minute_prev - Previous minute
 * @return true if update needed, false otherwise
 */
bool needsUpdate_Time(byte hour_curr, byte minute_curr, byte hour_prev, byte minute_prev) {
  return (hour_curr != hour_prev || minute_curr != minute_prev);
}

/**
 * needsUpdate_ModeChange - Check if display mode/menu changed
 * 
 * Compares current and previous display array to detect mode changes
 * 
 * @param current - Pointer to current display array
 * @param previous - Pointer to previous display array
 * @param size - Size of array to compare
 * @return true if mode changed, false otherwise
 */
bool needsUpdate_ModeChange(byte* current, byte* previous, int size) {
  for (int i = 0; i < size; i++) {
    if (current[i] != previous[i]) {
      return true;
    }
  }
  return false;
}

/**
 * getDisplayUpdateInterval - Get appropriate refresh rate for display mode
 * 
 * Variable refresh rates based on content type:
 * - 83ms (12Hz): RPM - needs fast updates for responsiveness
 * - 143ms (7Hz): Pressures, speed, AFR, ignition, injector - moderate updates
 * - 500ms (2Hz): Temps, battery, fuel level, clock, odometer - slow changing values
 * - 1000ms (1Hz): Static logos - minimal updates (check for mode change only)
 * 
 * @param displayMode - The display mode/case number
 * @param displayNum - Which display: 1 or 2
 * @return Update interval in milliseconds
 */
unsigned int getDisplayUpdateInterval(byte displayMode, byte displayNum) {
  // Display 1 refresh rates
  if (displayNum == 1) {
    switch (displayMode) {
      case 9:   // RPM
      case 16:  // Boost gauge with bar (12Hz, same as RPM)
      case 17:  // Boost text display (12Hz, same as RPM)
        return 83;
      
      case 8:   // Speed
      case 10:  // Ignition angle
      case 11:  // AFR
      case 12:  // Fuel pressure
      case 14:  // Injector duty
        return 143;
      
      case 0:   // Settings menu
      case 1:   // Oil Pressure
      case 2:   // Coolant Temp
      case 3:   // Oil Temp
      case 4:   // Fuel Level
      case 5:   // Battery Voltage
      case 6:   // Clock
      case 7:   // Trip Odometer
      case 13:  // Fuel composition
        return 500;
      
      case 15:  // Falcon Script logo
        return 1000;
      
      default:
        return 143;
    }
  }
  // Display 2 refresh rates
  else {
    switch (displayMode) {
      case 4:   // RPM
      case 10:  // Boost gauge with bar (12Hz, same as RPM)
      case 11:  // Boost text display (12Hz, same as RPM)
        return 83;
      
      case 5:   // Speed
        return 143;
      
      case 0:   // Oil Pressure
      case 1:   // Coolant Temp
      case 2:   // Battery Voltage
      case 3:   // Fuel Level
      case 6:   // Clock
        return 500;
      
      case 7:   // 302CID logo
      case 8:   // 302V logo
      case 9:   // Falcon Script logo
        return 1000;
      
      default:
        return 143;
    }
  }
}
