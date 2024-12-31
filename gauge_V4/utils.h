#ifndef UTILS_H
#define UTILS_H

void sigSelect();

void useInterrupt(boolean v);
void ledShiftLight(int ledRPM);
void motorZeroSynchronous();
void motorSweepSynchronous();
void generateRPM();
void serialInputFunc();
void shutdown();
int speedometerAngle(int sweep);
int fuelLvlAngle(int sweep);
int coolantTempAngle(int sweep);

#endif // UTILS_H