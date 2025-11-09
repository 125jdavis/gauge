/*
 * ========================================
 * MENU SYSTEM FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of menu and encoder functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "MenuSystem.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"
#include "DisplayFunctions.h"
#include <EEPROM.h>

void swRead() {       
  stateSW = digitalRead(SWITCH);            // Read current state of encoder button
  int stateChange = stateSW - lastStateSW;  // Calculate change: -1=pressed, +1=released, 0=no change

  // Clear debounce flag if enough time has passed since last change
  if ((millis() - lastStateChangeTime) > debounceDelay) {
    debounceFlag = 0;  // Allow new state changes to be registered
  }

  // Detect button press (falling edge) - record time but don't trigger action yet
  if (stateChange < 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of press
    debounceFlag = 1;                // Block bounces
  } 
  // Detect button release (rising edge) - this is when we register the button press
  else if (stateChange > 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of release
    debounceFlag = 1;                // Block bounces
    button = 1;                      // Set button event flag (cleared by menu handlers)
  } 
  else if (stateChange = 0) {  
    // No state change - do nothing
  }
  lastStateSW = stateSW;  // Save current state for next comparison
}

/**
 * rotate - Rotary encoder interrupt handler
 * 
 * Called by hardware interrupt whenever encoder rotates (CHANGE on pins 2 or 3).
 * Updates menu position (dispArray1[menuLevel]) based on rotation direction.
 * Wraps around at menu boundaries for continuous scrolling.
 * 
 * The Rotary library handles quadrature decoding and debouncing, providing
 * clean DIR_CW (clockwise) and DIR_CCW (counter-clockwise) events.
 * 
 * Global variables modified:
 * - dispArray1[menuLevel]: Incremented or decremented based on rotation
 * 
 * Interrupt context: Keep this function fast and simple
 */

void rotate() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  
  if (result == DIR_CW) {
    // Clockwise rotation - increment menu position
    if (dispArray1[menuLevel] == nMenuLevel) 
      dispArray1[menuLevel] = 0;  // Wrap to beginning
    else 
      dispArray1[menuLevel]++;    // Move to next item
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise rotation - decrement menu position
    if (dispArray1[menuLevel] == 0) 
      dispArray1[menuLevel] = nMenuLevel;  // Wrap to end
    else 
      dispArray1[menuLevel]--;              // Move to previous item
  }
}

/**
 * dispMenu - Multi-level menu system controller for display 1
 * 
 * This function implements a hierarchical menu system using a switch-case structure.
 * Menu navigation is controlled by:
 * - Rotary encoder rotation: changes dispArray1[menuLevel]
 * - Button press: enters submenu or triggers action
 * 
 * Menu structure (dispArray1 is [level0, level1, level2, level3]):
 * - Level 0: Main screens (oil pressure, coolant temp, oil temp, fuel, battery, clock, trip odo, speed, RPM, etc.)
 * - Level 1: Submenus (settings options, trip odo reset options)
 * - Level 2: Sub-submenus (display selections, unit selections, clock offset)
 * - Level 3: Final selection values
 * 
 * The menuLevel variable tracks current depth, and nMenuLevel defines how many
 * options exist at the current level (for wrapping rotation).
 * 
 * Global variables used:
 * - dispArray1[4]: Current position in menu hierarchy
 * - menuLevel: Current menu depth (0-3)
 * - nMenuLevel: Number of items in current menu
 * - button: Button press flag (cleared after handling)
 * 
 * Special behaviors:
 * - Case 0 (Settings) is always last for easy access
 * - Case 7 (Trip Odo) has reset confirmation submenu
 * - EEPROM updates happen when exiting settings
 * 
 * Called from: main loop at dispUpdateRate (75ms)
 */

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
 * goToLevel0 - Reset menu navigation to top level
 * 
 * Returns to the main menu (level 0) and resets all menu position variables.
 * This function is called when exiting settings or after making a selection.
 * 
 * Actions performed:
 * - Clear button flag
 * - Reset all menu levels to position 0 (Settings screen)
 * - Set menuLevel to 0 (top level)
 * - Set nMenuLevel to 14 (15 main menu items, 0-indexed)
 * 
 * Note: dispArray1[0] = 0 positions cursor on Settings, which is case 0 (last screen)
 * 
 * Called from: Settings submenus, trip odo reset confirmation
 */

void goToLevel0(void){
  button = 0;           // Clear button press flag
  dispArray1[0] = 0;    // Set to Settings screen (case 0)
  dispArray1[1] = 0;    // Clear level 1 selection
  dispArray1[2] = 0;    // Clear level 2 selection
  menuLevel = 0;        // Return to top menu level
  nMenuLevel = 14;      // Set to 15 items in main menu (0-indexed)
}

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
 * 
 * Called from: main loop at dispUpdateRate (75ms)
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

/**
 * incrementOffset - Rotary encoder handler for clock offset adjustment
 * 
 * Special interrupt handler used only when adjusting clock offset.
 * Replaces the normal 'rotate' handler temporarily.
 * Adjusts clockOffset variable directly (0-23 hours) with wraparound.
 * 
 * Range: 0-23 (24-hour format for UTC offset)
 * Example: UTC+5 would be clockOffset = 5
 *          UTC-8 would be clockOffset = 16 (24-8)
 * 
 * Global variables modified:
 * - clockOffset: Hours to add to GPS UTC time (0-23, wraps around)
 * 
 * Interrupt context: Keep function fast and simple
 */

void incrementOffset() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  
  if (result == DIR_CW) {
    // Clockwise - increment offset with wraparound
    if (clockOffset == 23) 
      clockOffset = 0;  // Wrap to 0
    else 
      clockOffset++;    // Increment
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise - decrement offset with wraparound
    if(clockOffset == 0) 
      clockOffset = 23;  // Wrap to 23
    else 
      clockOffset--;     // Decrement
  }
}


/*
 * ========================================
 * SCREEN DRAWING FUNCTIONS
 * ========================================
 * 
 * These functions render specific screens and data on the OLED displays.
 * Each function takes a pointer to an Adafruit_SSD1306 display object,
 * allowing the same function to draw on either display1 or display2.
 * 
 * General structure:
 * 1. Set text color (WHITE for visible pixels on OLED)
 * 2. Clear display buffer
 * 3. Set text size and cursor position
 * 4. Print text and/or draw graphics
 * 5. Call display() to transfer buffer to screen
 * 
 * Layout considerations:
 * - Screen is 128x32 pixels
 * - Text size 1: 6x8 pixels per character
 * - Text size 2: 12x16 pixels per character  
 * - Text size 3: 18x24 pixels per character
 * - Center position calculated as: center - (numDigits * pixelsPerChar / 2)
 */

/**
 * dispSettings - Display "SETTINGS" header screen
 * Shows the main settings menu title with a border frame
 */