#include "WifiPortalManager.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

bool wifi_has_credentials() {
    return WiFi.SSID().length() > 0;
}

WifiResult wifi_connect(int max_attempts, int attempt_timeout_s, int delay_between_s) {
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        Serial.printf("[WiFi] Attempt %d/%d connecting...\n", attempt, max_attempts);
        WiFi.begin();  // no args — uses saved credentials
        unsigned long deadline = millis() + (unsigned long)attempt_timeout_s * 1000;
        while (millis() < deadline) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi] Attempt %d/%d connected\n", attempt, max_attempts);
                return WIFI_OK;
            }
            delay(100);
            yield();
        }
        Serial.printf("[WiFi] Attempt %d/%d failed\n", attempt, max_attempts);
        if (attempt < max_attempts) delay((unsigned long)delay_between_s * 1000);
    }
    Serial.println("[WiFi] All attempts exhausted — connection failed");
    return WIFI_FAILED;
}

WifiResult wifi_open_portal(WiFiManager& mgr, const char* ap_name, int timeout_s,
                             void (*led_tick)(uint32_t)) {
    mgr.setConfigPortalBlocking(false);
    mgr.setConfigPortalTimeout(timeout_s);
    mgr.startConfigPortal(ap_name);
    Serial.printf("[WiFi] Portal started: %s (timeout %ds)\n", ap_name, timeout_s);

    while (mgr.getConfigPortalActive()) {
        mgr.process();
        led_tick(millis());
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Portal: saved and connected");
        return PORTAL_SAVED;
    }
    Serial.println("[WiFi] Portal: timed out");
    return PORTAL_TIMEOUT;
}
