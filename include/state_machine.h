#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "config.h"

void stateMachineInit();
void stateMachineUpdate(bool motionDetected);
SystemState getCurrentState();
unsigned long getTimeSinceLastMotion();

#endif // STATE_MACHINE_H
