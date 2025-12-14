#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <stdint.h>
#include "esp_err.h"

#include "gauge_config.h"

void mqtt_app_start(void);
void mqtt_publish_gauge_data(float *values, GasGaugeConfig *configs, int count);
bool mqtt_is_connected(void);

#endif
