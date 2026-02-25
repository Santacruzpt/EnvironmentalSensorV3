#include "MqttClient.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

bool mqtt_connect(PubSubClient& client,
                  const char* server, int port,
                  const char* client_id,
                  const char* username, const char* password,
                  const char* lwt_topic,
                  int max_attempts, int attempt_timeout_s) {
    static WiFiClient wifi_client;
    client.setClient(wifi_client);
    client.setServer(server, port);
    client.setKeepAlive(60);

    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        Serial.printf("[MQTT] Connect attempt %d/%d...\n", attempt, max_attempts);
        client.connect(client_id, username, password,
                       lwt_topic, 1, true, "OFFLINE");
        unsigned long deadline = millis() + (unsigned long)attempt_timeout_s * 1000;
        while (millis() < deadline) {
            if (client.connected()) {
                Serial.println("[MQTT] Connected");
                return true;
            }
            delay(100);
            yield();
        }
        Serial.printf("[MQTT] Attempt %d failed, state=%d\n", attempt, client.state());
        if (attempt < max_attempts) delay(2000);
    }
    return false;
}

bool mqtt_publish_status(PubSubClient& client, const char* topic, const char* payload) {
    // PubSubClient publish() only supports QoS 0; retain=true as required
    return client.publish(topic, payload, true);
}

bool mqtt_publish_measurement(PubSubClient& client, const char* topic, const char* payload) {
    return client.publish(topic, payload, true);
}

void mqtt_flush_and_disconnect(PubSubClient& client) {
    client.loop();
    delay(100);
    client.disconnect();
}
