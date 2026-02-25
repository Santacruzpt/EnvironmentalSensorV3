#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif

inline void format_device_name(uint32_t chip_id, char* buf, size_t len) {
    snprintf(buf, len, "esp-%06x", chip_id);
}

inline void build_topic(const char* root, const char* device,
                        const char* sub, char* buf, size_t len) {
    snprintf(buf, len, "%s/%s/%s", root, device, sub);
}

inline void format_float_1dp(float val, char* buf, size_t len) {
    snprintf(buf, len, "%.1f", val);
}

inline void build_telemetry_topic(const char* root, const char* device,
                                  const char* sub, char* buf, size_t len) {
    snprintf(buf, len, "%s/%s/telemetry/%s", root, device, sub);
}

// returns "BAT_CRIT", "BAT_LOW", or nullptr (no battery issue)
inline const char* battery_status_str(float battery_v, float low_v, float critical_v) {
    if (battery_v <= critical_v) return "BAT_CRIT";
    if (battery_v <= low_v)      return "BAT_LOW";
    return nullptr;
}
