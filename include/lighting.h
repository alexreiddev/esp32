#ifndef LIGHTING_H
#define LIGHTING_H

#include "config.h"

struct Color {
    float r;
    float g;
    float b;
};

void lightingInit();
void lightingUpdate();
void setTargetColor(uint8_t r, uint8_t g, uint8_t b);
void setTargetFromDistance(float distanceCm);
void setBrightnessScale(float scale);  // 0.0 - 1.0
void setOverrideColor(uint8_t r, uint8_t g, uint8_t b);
void clearOverride();
bool isOverrideActive();
void lightsOff();
Color getCurrentColor();

#endif // LIGHTING_H
