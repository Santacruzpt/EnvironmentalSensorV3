#include "DhtSensor.h"
#include <Arduino.h>
#include <math.h>

bool dht_read_average(DHT& dht, int num_reads, float& temp_out, float& hum_out) {
    float temp_sum = 0.0f;
    float hum_sum = 0.0f;
    int valid_count = 0;

    for (int i = 0; i < num_reads; i++) {
        float t = dht.readTemperature();
        float h = dht.readHumidity();
        if (!isnan(t) && !isnan(h)) {
            temp_sum += t;
            hum_sum += h;
            valid_count++;
            Serial.printf("[DHT] Read %d/%d: %.1fC %.1f%%\n", i + 1, num_reads, t, h);
        } else {
            Serial.printf("[DHT] Read %d/%d: failed (NaN)\n", i + 1, num_reads);
        }
        if (i < num_reads - 1) delay(1000);  // 1s minimum DHT11 sampling interval
    }

    if (valid_count == 0) return false;

    temp_out = temp_sum / valid_count;
    hum_out = hum_sum / valid_count;
    Serial.printf("[DHT] Average: %.1fC %.1f%%\n", temp_out, hum_out);
    return true;
}
