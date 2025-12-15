#ifndef ARDUINO_IOT_BRIDGE_H
#define ARDUINO_IOT_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize Arduino Core and IoT Cloud
 */
void arduino_iot_init(void);

/**
 * @brief Update gauge value to be sent to cloud
 * @param index Gauge index (0-7 for Ch1-Ch8)
 * @param val Value to send
 */
void arduino_iot_update_gauge(int index, float val);

/**
 * @brief Task to run the Arduino Cloud loop
 * @param pvParameters unused
 */
void arduino_iot_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // ARDUINO_IOT_BRIDGE_H
