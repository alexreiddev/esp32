#ifndef SENSOR_H
#define SENSOR_H

#include "config.h"

void sensorInit();
float getDistanceCm();
float getSmoothedDistance();
bool isMotionDetected();
float getAmbientBrightness();  // 0.0 (dark) to 1.0 (bright)

// ISR for PIR
void IRAM_ATTR pirISR();

#endif // SENSOR_H
