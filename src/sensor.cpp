#include "sensor.h"

// --- Moving average buffer ---
static float distanceBuffer[DISTANCE_SAMPLE_COUNT];
static int bufferIndex = 0;
static bool bufferFilled = false;

// --- Exponential smoothing state ---
static float smoothedDistance = 0.0f;
static bool smoothedInit = false;

// --- LDR smoothing ---
static float smoothedLDR = 0.0f;
static bool ldrInit = false;

// --- PIR state (volatile for ISR) ---
static volatile bool motionFlag = false;
static volatile unsigned long lastMotionTime = 0;

void IRAM_ATTR pirISR() {
    motionFlag = true;
    lastMotionTime = millis();
}

void sensorInit() {
    // Ultrasonic pins
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);

    // PIR with interrupt
    pinMode(PIN_PIR, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_PIR), pirISR, RISING);

    // LDR is analog input, no setup needed on ESP32 (ADC)

    // Initialize buffers
    for (int i = 0; i < DISTANCE_SAMPLE_COUNT; i++) {
        distanceBuffer[i] = 0.0f;
    }
}

float getDistanceCm() {
    // Non-blocking pulse timing
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    unsigned long duration = pulseIn(PIN_ECHO, HIGH, DISTANCE_TIMEOUT_US);

    if (duration == 0) {
        // Timeout — return last smoothed value to avoid spikes
        return smoothedDistance;
    }

    float distance = (duration * 0.0343f) / 2.0f;
    return distance;
}

static float computeMovingAverage(float newSample) {
    distanceBuffer[bufferIndex] = newSample;
    bufferIndex = (bufferIndex + 1) % DISTANCE_SAMPLE_COUNT;
    if (bufferIndex == 0) bufferFilled = true;

    int count = bufferFilled ? DISTANCE_SAMPLE_COUNT : bufferIndex;
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += distanceBuffer[i];
    }
    return sum / count;
}

float getSmoothedDistance() {
    float raw = getDistanceCm();

    // Stage 1: Moving average
    float averaged = computeMovingAverage(raw);

    // Stage 2: Exponential smoothing
    if (!smoothedInit) {
        smoothedDistance = averaged;
        smoothedInit = true;
    } else {
        smoothedDistance = EXP_SMOOTHING_ALPHA * averaged
                        + (1.0f - EXP_SMOOTHING_ALPHA) * smoothedDistance;
    }

    return smoothedDistance;
}

bool isMotionDetected() {
    if (motionFlag) {
        motionFlag = false;
        return true;
    }
    // Also check the pin directly for sustained HIGH
    return digitalRead(PIN_PIR) == HIGH;
}

float getAmbientBrightness() {
    int raw = analogRead(PIN_LDR);

    float normalized = (float)raw / (float)LDR_MAX_RAW;

    // Exponential smoothing
    if (!ldrInit) {
        smoothedLDR = normalized;
        ldrInit = true;
    } else {
        smoothedLDR = LDR_SMOOTHING_ALPHA * normalized
                    + (1.0f - LDR_SMOOTHING_ALPHA) * smoothedLDR;
    }

    return smoothedLDR;
}
