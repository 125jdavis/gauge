#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GPS.h> //https://github.com/adafruit/Adafruit_GPS

#define GPSECHO  false        // do not send raw GPS data to serial monitor 

void fetchGPSdata();
SIGNAL(TIMER0_COMPA_vect);