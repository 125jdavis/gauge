#ifndef DISPLAY_H
#define DISPLAY_H

#define GPSECHO  false        // do not send raw GPS data to serial monitor 

void fetchGPSdata();
SIGNAL(TIMER0_COMPA_vect);