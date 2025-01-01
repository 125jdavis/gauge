#ifndef IMAGES_H
#define IMAGES_H

#include <avr/pgmspace.h>  // Necessary for storing data in program memory (PROGMEM) but do i really need it??

extern const unsigned char img_falcon_script[];
extern const unsigned char img_302_CID[];
extern const unsigned char img_302V[];
extern const unsigned char img_oilPrs[];
extern const unsigned char img_oilTemp[];
extern const unsigned char img_battVolt[];
extern const unsigned char img_coolantTemp[];
extern const unsigned char img_fuelLvl[];

#endif // IMAGES_H