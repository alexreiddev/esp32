#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// Pin Configuration
// ============================================================

// Ultrasonic sensor (HC-SR04)
#define PIN_TRIG          5
#define PIN_ECHO          18

// RGB LED strip PWM outputs (via IRFZ44N MOSFETs)
#define PIN_LED_RED       25
#define PIN_LED_GREEN     26
#define PIN_LED_BLUE      27

// PIR motion sensor
#define PIN_PIR           19

// LDR ambient light sensor (analog)
#define PIN_LDR           34

// ============================================================
// PWM Configuration
// ============================================================

#define PWM_FREQ          5000    // 5 kHz PWM frequency
#define PWM_RESOLUTION    8       // 8-bit resolution (0-255)
#define PWM_CHANNEL_RED   0
#define PWM_CHANNEL_GREEN 1
#define PWM_CHANNEL_BLUE  2

// ============================================================
// Distance Sensor Configuration
// ============================================================

#define DISTANCE_MIN_CM         20    // Closest mapped distance
#define DISTANCE_MAX_CM         120   // Farthest mapped distance
#define DISTANCE_TIMEOUT_US     25000 // Pulse timeout (~4.3m max)
#define DISTANCE_SAMPLE_COUNT   5     // Moving average window size

// Exponential smoothing factor (0.0 - 1.0, higher = less smoothing)
#define EXP_SMOOTHING_ALPHA     0.3f

// ============================================================
// Color Mapping
// ============================================================

// Near color (warm red)
#define COLOR_NEAR_R      255
#define COLOR_NEAR_G      0
#define COLOR_NEAR_B      0

// Far color (cool blue)
#define COLOR_FAR_R       0
#define COLOR_FAR_G       0
#define COLOR_FAR_B       255

// ============================================================
// Fade Engine
// ============================================================

#define FADE_INTERVAL_MS        16    // ~60 FPS target
#define FADE_SPEED              0.08f // Interpolation speed per frame (0.0-1.0)

// ============================================================
// Gamma Correction
// ============================================================

#define GAMMA_VALUE             2.2f

// ============================================================
// State Machine Timing
// ============================================================

#define IDLE_TIMEOUT_MS         60000   // 60s no motion -> IDLE
#define SLEEP_TIMEOUT_MS        300000  // 5min inactivity -> SLEEP

// ============================================================
// Ambient Light (LDR)
// ============================================================

#define LDR_MIN_RAW             0       // Darkest reading
#define LDR_MAX_RAW             4095    // Brightest reading
#define LDR_SMOOTHING_ALPHA     0.1f    // Slow response for ambient
#define BRIGHTNESS_MIN_SCALE    0.05f   // Minimum brightness floor
#define BRIGHTNESS_MAX_SCALE    1.0f    // Maximum brightness

// ============================================================
// WiFi Configuration
// ============================================================

#define WIFI_SSID               "ESP32_Light"
#define WIFI_PASSWORD           "adaptive123"
#define WIFI_AP_MODE            true    // true=AP, false=STA
#define WEB_SERVER_PORT         80

// ============================================================
// RTOS Configuration
// ============================================================

#define SENSOR_TASK_STACK       4096
#define LIGHTING_TASK_STACK     4096
#define WIFI_TASK_STACK         8192
#define POWER_TASK_STACK        2048

#define SENSOR_TASK_PRIORITY    2
#define LIGHTING_TASK_PRIORITY  3       // Highest - smooth visuals
#define WIFI_TASK_PRIORITY      1
#define POWER_TASK_PRIORITY     1

#define SENSOR_TASK_INTERVAL_MS   50    // 20 Hz sensor polling
#define POWER_TASK_INTERVAL_MS    1000  // 1 Hz power check

// ============================================================
// State Enum
// ============================================================

enum SystemState {
    STATE_ACTIVE,
    STATE_IDLE,
    STATE_SLEEP
};

#endif // CONFIG_H
