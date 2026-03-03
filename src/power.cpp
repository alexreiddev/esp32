#include "power.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

void powerInit() {
    // Configure PIR pin as wakeup source for deep sleep
    // GPIO 19 is an RTC GPIO, suitable for ext0 wakeup
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PIR, 1); // Wake on HIGH
}

void powerUpdate(SystemState state) {
    switch (state) {
        case STATE_ACTIVE:
            // Full power, nothing to do
            break;

        case STATE_IDLE:
            // Light sleep — CPU pauses, peripherals stay on, fast wake
            enterLightSleep();
            break;

        case STATE_SLEEP:
            // Deep sleep — minimal power, PIR interrupt wakes
            enterDeepSleep();
            break;
    }
}

void enterLightSleep() {
    // Configure timer wakeup (wake periodically to check sensors)
    esp_sleep_enable_timer_wakeup(1000000); // 1 second
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PIR, 1);
    esp_light_sleep_start();
}

void enterDeepSleep() {
    // Turn off LEDs before sleeping
    ledcWrite(PWM_CHANNEL_RED, 0);
    ledcWrite(PWM_CHANNEL_GREEN, 0);
    ledcWrite(PWM_CHANNEL_BLUE, 0);

    // Only PIR wakeup for deep sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PIR, 1);
    esp_deep_sleep_start();
    // Execution stops here; on wake, ESP32 reboots
}
