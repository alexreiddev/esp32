#ifndef POWER_H
#define POWER_H

#include "config.h"

void powerInit();
void powerUpdate(SystemState state);
void enterLightSleep();
void enterDeepSleep();

#endif // POWER_H
