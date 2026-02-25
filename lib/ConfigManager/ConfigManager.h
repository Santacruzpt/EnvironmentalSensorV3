#pragma once

#include <stddef.h>

struct Config {
    bool wifi_reset;
    char mqtt_server[64];
    int mqtt_port;
    char mqtt_topic_root[64];
    char mqtt_username[64];
    char mqtt_password[64];
    int sleep_normal_s;
    int sleep_low_battery_s;
    int sleep_critical_battery_s;
    float battery_low_v;
    float battery_critical_v;
};

bool config_load(Config& cfg);
// Returns true on success. Returns false if:
//   - LittleFS fails to mount
//   - /config.json not found
//   - /config.json contains invalid JSON
// On missing keys only: fills defaults (via config_apply_defaults) and returns true.

void config_save(const Config& cfg);
// Writes complete config.json to LittleFS at /config.json using ArduinoJson.

void config_apply_defaults(Config& cfg);
// Unconditionally sets ALL fields to their hardcoded defaults.
// Called by config_load before JSON parsing so JSON values overwrite defaults.
// Hardcoded defaults:
//   wifi_reset             = false
//   mqtt_server            = "" (empty)
//   mqtt_port              = 1883
//   mqtt_topic_root        = "devices"
//   mqtt_username          = "" (empty)
//   mqtt_password          = "" (empty)
//   sleep_normal_s         = 60
//   sleep_low_battery_s    = 300
//   sleep_critical_battery_s = 86400
//   battery_low_v          = 3.5f
//   battery_critical_v     = 3.40f
