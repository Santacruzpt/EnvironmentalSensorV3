#pragma once

#include <Arduino.h>

void led_init();
// Configures LED_BUILTIN as OUTPUT and sets it HIGH (off, active LOW).

void led_off();
// Sets LED_BUILTIN HIGH (off).

void led_update_wifi(uint32_t now);
// Non-blocking: 500ms on / 500ms off repeating toggle.
// Call repeatedly in a loop, passing millis().

void led_update_sensor(uint32_t now);
// Non-blocking: 500ms on / 500ms off / 500ms on / 1000ms off repeating.
// Call repeatedly in a loop, passing millis().

void led_update_mqtt(uint32_t now);
// Non-blocking: 500ms on / 500ms off / 1000ms on repeating.
// Call repeatedly in a loop, passing millis().

void led_update_portal(uint32_t now);
// Non-blocking: 1000ms on / 1000ms off repeating toggle.
// Call repeatedly in a loop, passing millis().

void led_error_blocking(int duration_ms);
// Blocking: 100ms on / 100ms off for duration_ms total.
// Uses delay() to feed the ESP8266 software watchdog.
