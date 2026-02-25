#pragma once

#include <DHT.h>

bool dht_read_average(DHT& dht, int num_reads,
                      float& temp_out, float& hum_out);
// Takes num_reads readings, 1s apart (delay(1000) between).
// Discards NaN results (isnan() check).
// Sets temp_out and hum_out to the average of valid reads.
// Returns true if at least one valid read; false if all reads failed.
