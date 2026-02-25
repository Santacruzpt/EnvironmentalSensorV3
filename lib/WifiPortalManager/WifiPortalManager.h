#pragma once

#include <WiFiManager.h>

enum WifiResult {
    WIFI_OK,
    WIFI_FAILED,
    PORTAL_SAVED,
    PORTAL_TIMEOUT
};

bool wifi_has_credentials();
// Returns true if ESP8266 has saved WiFi credentials: WiFi.SSID().length() > 0

WifiResult wifi_connect(int max_attempts, int attempt_timeout_s, int delay_between_s);
// Attempts WiFi connection using saved credentials (WiFi.begin() with no args).
// Polls WiFi.status() with millis() deadline per attempt.
// Returns WIFI_OK on success, WIFI_FAILED if all attempts exhausted.
// Never calls autoConnect().

WifiResult wifi_open_portal(WiFiManager& mgr, const char* ap_name, int timeout_s,
                             void (*led_tick)(uint32_t));
// Opens WiFiManager captive portal in non-blocking mode.
// Caller must have registered setSaveConfigCallback() before calling this.
// Sets setConfigPortalBlocking(false) and setConfigPortalTimeout(timeout_s).
// Loops calling mgr.process() with LED pattern until saved or timed out.
// led_tick is called each loop iteration with millis() for LED control.
// Returns PORTAL_SAVED or PORTAL_TIMEOUT.
