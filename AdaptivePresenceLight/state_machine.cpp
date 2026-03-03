#include "state_machine.h"

static SystemState currentState = STATE_ACTIVE;
static unsigned long lastMotionTime = 0;

void stateMachineInit() {
    currentState = STATE_ACTIVE;
    lastMotionTime = millis();
}

void stateMachineUpdate(bool motionDetected) {
    unsigned long now = millis();

    if (motionDetected) {
        lastMotionTime = now;
        currentState = STATE_ACTIVE;
        return;
    }

    unsigned long elapsed = now - lastMotionTime;

    switch (currentState) {
        case STATE_ACTIVE:
            if (elapsed >= IDLE_TIMEOUT_MS) {
                currentState = STATE_IDLE;
            }
            break;

        case STATE_IDLE:
            if (elapsed >= SLEEP_TIMEOUT_MS) {
                currentState = STATE_SLEEP;
            }
            break;

        case STATE_SLEEP:
            // Remain in sleep until motion wakes us
            break;
    }
}

SystemState getCurrentState() {
    return currentState;
}

unsigned long getTimeSinceLastMotion() {
    return millis() - lastMotionTime;
}
