#pragma once

#include <PubSubClient.h>

bool mqtt_connect(PubSubClient& client,
                  const char* server, int port,
                  const char* client_id,
                  const char* username, const char* password,
                  const char* lwt_topic,
                  int max_attempts, int attempt_timeout_s);
// Connects to MQTT broker with LWT.
// Sets client.setKeepAlive(60) before connecting.
// LWT: lwt_topic, payload "OFFLINE", QoS 1, retain true.
// Retry loop: up to max_attempts, millis() deadline per attempt, 2s delay between.
// Returns true on success.

bool mqtt_publish_status(PubSubClient& client, const char* topic, const char* payload);
// Publishes to topic with QoS 1, retain true.
// Returns client.publish() result.

bool mqtt_publish_measurement(PubSubClient& client, const char* topic, const char* payload);
// Publishes to topic with QoS 0, retain true.
// Returns client.publish() result.

void mqtt_flush_and_disconnect(PubSubClient& client);
// Calls client.loop() then delay(100) then client.disconnect().
