#include "LedIndicator.h"

void led_init() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // active LOW — HIGH = off
}

void led_off() {
    digitalWrite(LED_BUILTIN, HIGH); // active LOW — HIGH = off
}

// 500ms on / 500ms off, repeating (period = 1000ms)
void led_update_wifi(uint32_t now) {
    static uint32_t phase_start = 0;
    static uint8_t phase = 0;
    static const uint16_t durations[2] = {500, 500};

    if (now - phase_start >= durations[phase]) {
        phase = (phase + 1) % 2;
        phase_start = now;
    }
    // phase 0 = on, phase 1 = off
    bool on = (phase == 0);
    digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

// 500ms on / 500ms off / 500ms on / 1000ms off, repeating (period = 2500ms)
void led_update_sensor(uint32_t now) {
    static uint32_t phase_start = 0;
    static uint8_t phase = 0;
    static const uint16_t durations[4] = {500, 500, 500, 1000};

    if (now - phase_start >= durations[phase]) {
        phase = (phase + 1) % 4;
        phase_start = now;
    }
    // phases 0 and 2 = on, phases 1 and 3 = off
    bool on = (phase == 0 || phase == 2);
    digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

// 500ms on / 500ms off / 1000ms on, repeating (period = 2000ms)
void led_update_mqtt(uint32_t now) {
    static uint32_t phase_start = 0;
    static uint8_t phase = 0;
    static const uint16_t durations[3] = {500, 500, 1000};

    if (now - phase_start >= durations[phase]) {
        phase = (phase + 1) % 3;
        phase_start = now;
    }
    // phase 1 = off, phases 0 and 2 = on
    bool on = (phase != 1);
    digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

// 1000ms on / 1000ms off, repeating (period = 2000ms)
void led_update_portal(uint32_t now) {
    static uint32_t phase_start = 0;
    static uint8_t phase = 0;
    static const uint16_t durations[2] = {1000, 1000};

    if (now - phase_start >= durations[phase]) {
        phase = (phase + 1) % 2;
        phase_start = now;
    }
    // phase 0 = on, phase 1 = off
    bool on = (phase == 0);
    digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

// Blocking: 100ms on / 100ms off for duration_ms total.
// Uses delay() to feed the ESP8266 software watchdog (yield() is called inside delay()).
void led_error_blocking(int duration_ms) {
    unsigned long end_time = millis() + (unsigned long)duration_ms;
    bool led_on = true;
    while (millis() < end_time) {
        digitalWrite(LED_BUILTIN, led_on ? LOW : HIGH);
        led_on = !led_on;
        delay(100); // feeds watchdog via yield() inside delay()
    }
    // Leave LED state as-is; caller must call led_off() before sleep
}
