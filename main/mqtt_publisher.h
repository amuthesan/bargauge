#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <stdint.h>
#include "esp_err.h"

void mqtt_app_start(void);
void mqtt_publish_gauge_data(float *values, int count);
bool mqtt_is_connected(void);

#endif
