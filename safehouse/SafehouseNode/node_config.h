#pragma once

// Which chapter this board serves (1..NODE_COUNT). PlatformIO envs set it with
// -DNODE_INDEX=n; in the Arduino IDE edit the default below before flashing.
#ifndef NODE_INDEX
#define NODE_INDEX 1
#endif

// --- Access point ---
#define AP_CHANNEL         6
#define AP_MAX_CLIENTS     8      // ESP32 soft-AP hard limit is 10
#define AP_IP              IPAddress(10, 7, 0, 1)
#define AP_NETMASK         IPAddress(255, 255, 255, 0)

// --- Answer rate limiting (per client IP) ---
#define MAX_WRONG_ATTEMPTS 5
#define LOCKOUT_MS         60000UL

// --- Operator stats page: http://10.7.0.1/ops?key=... ---
// Change this before deploying; it guards only visit/unlock counters.
#define OPS_KEY            "change-me"

// Status LED (built-in LED on most ESP32 dev boards). Set to -1 to disable.
#define STATUS_LED_PIN     2
