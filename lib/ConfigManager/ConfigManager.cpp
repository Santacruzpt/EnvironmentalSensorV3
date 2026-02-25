#ifndef NATIVE_TEST

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"

void config_apply_defaults(Config& cfg) {
    cfg.wifi_reset = false;
    cfg.mqtt_server[0] = '\0';
    cfg.mqtt_port = 1883;
    strncpy(cfg.mqtt_topic_root, "devices", sizeof(cfg.mqtt_topic_root));
    cfg.mqtt_topic_root[sizeof(cfg.mqtt_topic_root) - 1] = '\0';
    cfg.mqtt_username[0] = '\0';
    cfg.mqtt_password[0] = '\0';
    cfg.sleep_normal_s = 60;
    cfg.sleep_low_battery_s = 300;
    cfg.sleep_critical_battery_s = 86400;
    cfg.battery_low_v = 3.5f;
    cfg.battery_critical_v = 3.2f;
}

bool config_load(Config& cfg) {
    if (!LittleFS.begin()) {
        Serial.println("[Config] ERROR: LittleFS mount failed");
        return false;
    }

    if (!LittleFS.exists("/config.json")) {
        Serial.println("[Config] ERROR: /config.json not found");
        return false;
    }

    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        Serial.println("[Config] ERROR: Failed to open /config.json");
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.print("[Config] ERROR: JSON parse failed: ");
        Serial.println(err.c_str());
        return false;
    }

    // Apply defaults first; JSON values will overwrite where present
    config_apply_defaults(cfg);

    if (doc.containsKey("wifi") && doc["wifi"].containsKey("reset"))
        cfg.wifi_reset = doc["wifi"]["reset"].as<bool>();

    if (doc.containsKey("mqtt")) {
        JsonObject mqtt = doc["mqtt"];
        if (mqtt.containsKey("server"))
            strlcpy(cfg.mqtt_server, mqtt["server"].as<const char*>(), sizeof(cfg.mqtt_server));
        if (mqtt.containsKey("port"))
            cfg.mqtt_port = mqtt["port"].as<int>();
        if (mqtt.containsKey("topic_root"))
            strlcpy(cfg.mqtt_topic_root, mqtt["topic_root"].as<const char*>(), sizeof(cfg.mqtt_topic_root));
        if (mqtt.containsKey("username"))
            strlcpy(cfg.mqtt_username, mqtt["username"].as<const char*>(), sizeof(cfg.mqtt_username));
        if (mqtt.containsKey("password"))
            strlcpy(cfg.mqtt_password, mqtt["password"].as<const char*>(), sizeof(cfg.mqtt_password));
    }

    if (doc.containsKey("sleep")) {
        JsonObject sleep_obj = doc["sleep"];
        if (sleep_obj.containsKey("normal_s"))
            cfg.sleep_normal_s = sleep_obj["normal_s"].as<int>();
        if (sleep_obj.containsKey("low_battery_s"))
            cfg.sleep_low_battery_s = sleep_obj["low_battery_s"].as<int>();
        if (sleep_obj.containsKey("critical_battery_s"))
            cfg.sleep_critical_battery_s = sleep_obj["critical_battery_s"].as<int>();
    }

    if (doc.containsKey("battery")) {
        JsonObject battery = doc["battery"];
        if (battery.containsKey("low_v"))
            cfg.battery_low_v = battery["low_v"].as<float>();
        if (battery.containsKey("critical_v"))
            cfg.battery_critical_v = battery["critical_v"].as<float>();
    }

    // Print loaded values (mask password)
    Serial.println("[Config] Loaded config:");
    Serial.print("  wifi.reset: ");          Serial.println(cfg.wifi_reset);
    Serial.print("  mqtt.server: ");         Serial.println(cfg.mqtt_server);
    Serial.print("  mqtt.port: ");           Serial.println(cfg.mqtt_port);
    Serial.print("  mqtt.topic_root: ");     Serial.println(cfg.mqtt_topic_root);
    Serial.print("  mqtt.username: ");       Serial.println(cfg.mqtt_username);
    Serial.print("  mqtt.password: ");
    Serial.println(cfg.mqtt_password[0] != '\0' ? "(set)" : "(empty)");
    Serial.print("  sleep.normal_s: ");      Serial.println(cfg.sleep_normal_s);
    Serial.print("  sleep.low_battery_s: "); Serial.println(cfg.sleep_low_battery_s);
    Serial.print("  sleep.critical_battery_s: "); Serial.println(cfg.sleep_critical_battery_s);
    Serial.print("  battery.low_v: ");       Serial.println(cfg.battery_low_v, 2);
    Serial.print("  battery.critical_v: ");  Serial.println(cfg.battery_critical_v, 2);

    if (cfg.sleep_normal_s > 4294) {
        Serial.println("[Config] WARNING: sleep.normal_s exceeds ESP8266 hardware limit (~4294s); device will wake earlier than configured");
    }

    return true;
}

void config_save(const Config& cfg) {
    LittleFS.begin();  // safe to call if already mounted

    StaticJsonDocument<512> doc;

    doc["wifi"]["reset"] = cfg.wifi_reset;
    doc["mqtt"]["server"] = cfg.mqtt_server;
    doc["mqtt"]["port"] = cfg.mqtt_port;
    doc["mqtt"]["topic_root"] = cfg.mqtt_topic_root;
    doc["mqtt"]["username"] = cfg.mqtt_username;
    doc["mqtt"]["password"] = cfg.mqtt_password;
    doc["sleep"]["normal_s"] = cfg.sleep_normal_s;
    doc["sleep"]["low_battery_s"] = cfg.sleep_low_battery_s;
    doc["sleep"]["critical_battery_s"] = cfg.sleep_critical_battery_s;
    doc["battery"]["low_v"] = cfg.battery_low_v;
    doc["battery"]["critical_v"] = cfg.battery_critical_v;

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("[Config] ERROR: Failed to open /config.json for writing");
        return;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("[Config] Config saved");
}

#endif // NATIVE_TEST
