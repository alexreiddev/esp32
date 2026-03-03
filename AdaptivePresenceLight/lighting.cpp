#include "lighting.h"
#include <math.h>

// --- Gamma lookup table (computed at init) ---
static uint8_t gammaTable[256];

// --- Current and target colors (float for smooth interpolation) ---
static Color currentColor = {0, 0, 0};
static Color targetColor  = {0, 0, 0};

// --- Brightness scaling from ambient sensor ---
static float brightnessScale = 1.0f;

// --- Manual override ---
static bool overrideActive = false;
static Color overrideColor = {0, 0, 0};

// --- Timing ---
static unsigned long lastFadeUpdate = 0;

static void buildGammaTable() {
    for (int i = 0; i < 256; i++) {
        float normalized = (float)i / 255.0f;
        float corrected = powf(normalized, GAMMA_VALUE);
        gammaTable[i] = (uint8_t)(corrected * 255.0f + 0.5f);
    }
}

static uint8_t gammaCorrect(uint8_t value) {
    return gammaTable[value];
}

static void writePWM(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(PWM_CHANNEL_RED,   gammaCorrect(r));
    ledcWrite(PWM_CHANNEL_GREEN, gammaCorrect(g));
    ledcWrite(PWM_CHANNEL_BLUE,  gammaCorrect(b));
}

void lightingInit() {
    // Build gamma LUT
    buildGammaTable();

    // Configure LEDC PWM channels
    ledcSetup(PWM_CHANNEL_RED,   PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_GREEN, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_BLUE,  PWM_FREQ, PWM_RESOLUTION);

    // Attach channels to pins
    ledcAttachPin(PIN_LED_RED,   PWM_CHANNEL_RED);
    ledcAttachPin(PIN_LED_GREEN, PWM_CHANNEL_GREEN);
    ledcAttachPin(PIN_LED_BLUE,  PWM_CHANNEL_BLUE);

    // Start with LEDs off
    writePWM(0, 0, 0);

    lastFadeUpdate = millis();
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

void lightingUpdate() {
    unsigned long now = millis();
    if (now - lastFadeUpdate < FADE_INTERVAL_MS) return;
    lastFadeUpdate = now;

    // Determine effective target
    Color effective;
    if (overrideActive) {
        effective = overrideColor;
    } else {
        effective = targetColor;
    }

    // Smooth interpolation toward target
    currentColor.r = lerpf(currentColor.r, effective.r, FADE_SPEED);
    currentColor.g = lerpf(currentColor.g, effective.g, FADE_SPEED);
    currentColor.b = lerpf(currentColor.b, effective.b, FADE_SPEED);

    // Apply brightness scaling
    uint8_t outR = (uint8_t)constrain(currentColor.r * brightnessScale, 0, 255);
    uint8_t outG = (uint8_t)constrain(currentColor.g * brightnessScale, 0, 255);
    uint8_t outB = (uint8_t)constrain(currentColor.b * brightnessScale, 0, 255);

    writePWM(outR, outG, outB);
}

void setTargetColor(uint8_t r, uint8_t g, uint8_t b) {
    targetColor.r = (float)r;
    targetColor.g = (float)g;
    targetColor.b = (float)b;
}

void setTargetFromDistance(float distanceCm) {
    // Clamp distance to range
    float d = constrain(distanceCm, DISTANCE_MIN_CM, DISTANCE_MAX_CM);

    // Normalize to 0.0 (near) - 1.0 (far)
    float t = (d - DISTANCE_MIN_CM) / (float)(DISTANCE_MAX_CM - DISTANCE_MIN_CM);

    // Linear interpolation between near and far colors
    targetColor.r = lerpf(COLOR_NEAR_R, COLOR_FAR_R, t);
    targetColor.g = lerpf(COLOR_NEAR_G, COLOR_FAR_G, t);
    targetColor.b = lerpf(COLOR_NEAR_B, COLOR_FAR_B, t);
}

void setBrightnessScale(float scale) {
    brightnessScale = constrain(scale, 0.0f, 1.0f);
}

void setOverrideColor(uint8_t r, uint8_t g, uint8_t b) {
    overrideColor.r = (float)r;
    overrideColor.g = (float)g;
    overrideColor.b = (float)b;
    overrideActive = true;
}

void clearOverride() {
    overrideActive = false;
}

bool isOverrideActive() {
    return overrideActive;
}

void lightsOff() {
    targetColor = {0, 0, 0};
    currentColor = {0, 0, 0};
    writePWM(0, 0, 0);
}

Color getCurrentColor() {
    return currentColor;
}
