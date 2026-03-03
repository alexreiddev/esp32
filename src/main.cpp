#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "lighting.h"
#include "state_machine.h"
#include "power.h"
#include "wifi_control.h"

// ============================================================
// Shared state (protected by mutex)
// ============================================================

static SemaphoreHandle_t dataMutex;

static float sharedDistance   = 0.0f;
static bool  sharedMotion    = false;
static float sharedAmbient   = 0.0f;
static SystemState sharedState = STATE_ACTIVE;

// ============================================================
// RTOS Tasks
// ============================================================

void sensorTask(void* param) {
    (void)param;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        float dist    = getSmoothedDistance();
        bool  motion  = isMotionDetected();
        float ambient = getAmbientBrightness();

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sharedDistance = dist;
            sharedMotion   = motion;
            sharedAmbient  = ambient;
            xSemaphoreGive(dataMutex);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_TASK_INTERVAL_MS));
    }
}

void lightingTask(void* param) {
    (void)param;

    for (;;) {
        float dist;
        float ambient;
        SystemState state;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            dist    = sharedDistance;
            ambient = sharedAmbient;
            state   = sharedState;
            xSemaphoreGive(dataMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // Only drive LEDs when not sleeping
        if (state == STATE_ACTIVE) {
            if (!isOverrideActive()) {
                setTargetFromDistance(dist);
            }
            // Scale brightness based on ambient light:
            // darker room -> brighter LEDs
            float scale = BRIGHTNESS_MAX_SCALE - ambient
                          * (BRIGHTNESS_MAX_SCALE - BRIGHTNESS_MIN_SCALE);
            scale = constrain(scale, BRIGHTNESS_MIN_SCALE, BRIGHTNESS_MAX_SCALE);
            setBrightnessScale(scale);
        } else if (state == STATE_IDLE) {
            // Dim to low level
            setBrightnessScale(BRIGHTNESS_MIN_SCALE);
        } else {
            // SLEEP — lights off handled before entering deep sleep
        }

        lightingUpdate();

        vTaskDelay(pdMS_TO_TICKS(FADE_INTERVAL_MS));
    }
}

void wifiTask(void* param) {
    (void)param;

    for (;;) {
        wifiUpdate();
        vTaskDelay(pdMS_TO_TICKS(10)); // Fast polling for responsive web UI
    }
}

void powerTask(void* param) {
    (void)param;
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        bool motion;
        SystemState state;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            motion = sharedMotion;
            xSemaphoreGive(dataMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // Update state machine
        stateMachineUpdate(motion);
        state = getCurrentState();

        // Update shared state
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sharedState = state;
            xSemaphoreGive(dataMutex);
        }

        // Handle power modes
        if (state == STATE_SLEEP) {
            lightsOff();
            Serial.println("Entering deep sleep...");
            delay(100); // Let serial flush
            enterDeepSleep();
            // Won't reach here — ESP32 reboots on wake
        } else if (state == STATE_IDLE) {
            // Light sleep is handled in power.cpp; wake after ~1s or PIR
            enterLightSleep();
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(POWER_TASK_INTERVAL_MS));
    }
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Adaptive Presence Light ===");
    Serial.println("Initializing...");

    // Create mutex
    dataMutex = xSemaphoreCreateMutex();

    // Initialize subsystems
    sensorInit();
    lightingInit();
    stateMachineInit();
    powerInit();
    wifiInit();

    Serial.println("Creating RTOS tasks...");

    // Create FreeRTOS tasks pinned to cores
    // Core 0: WiFi (Arduino WiFi stack runs on core 0)
    // Core 1: Sensor, Lighting, Power
    xTaskCreatePinnedToCore(sensorTask,   "Sensor",   SENSOR_TASK_STACK,
                            NULL, SENSOR_TASK_PRIORITY,   NULL, 1);
    xTaskCreatePinnedToCore(lightingTask,  "Lighting", LIGHTING_TASK_STACK,
                            NULL, LIGHTING_TASK_PRIORITY,  NULL, 1);
    xTaskCreatePinnedToCore(wifiTask,      "WiFi",     WIFI_TASK_STACK,
                            NULL, WIFI_TASK_PRIORITY,      NULL, 0);
    xTaskCreatePinnedToCore(powerTask,     "Power",    POWER_TASK_STACK,
                            NULL, POWER_TASK_PRIORITY,     NULL, 1);

    Serial.println("System ready.");
}

void loop() {
    // All work is done in RTOS tasks.
    // Yield the Arduino loop task to avoid wasting CPU.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
