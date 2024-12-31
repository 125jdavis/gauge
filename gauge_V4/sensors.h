#ifndef SENSORS_H
#define SENSORS_H

void readSensors();
unsigned long readSensor(int inputPin, int oldVal, int filt);
unsigned long read30PSIAsensor(int inputPin, int oldVal, int filt);
float readThermSensor(int inputPin, float oldVal, int filt);
float curveLookup(float input, float brkpts[], float curve[], int curveLength);

#endif // SENSORS_H