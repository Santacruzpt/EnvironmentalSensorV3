#pragma once

#include <stddef.h>
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
