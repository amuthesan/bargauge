/*
 * RX8025T-UC Real-Time Clock Driver
 * I2C RTC chip with battery backup
 */

#pragma once

#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// RX8025T I2C Address (7-bit)
#define RX8025T_I2C_ADDR 0x32

// RX8025T Register Addresses
#define RX8025T_REG_SECONDS    0x00
#define RX8025T_REG_MINUTES    0x01
#define RX8025T_REG_HOURS      0x02
#define RX8025T_REG_WEEKDAY    0x03
#define RX8025T_REG_DAY        0x04
#define RX8025T_REG_MONTH      0x05
#define RX8025T_REG_YEAR       0x06
#define RX8025T_REG_DIGOFFSET  0x07
#define RX8025T_REG_ALARMIN    0x08
#define RX8025T_REG_CONTROL1   0x0E
#define RX8025T_REG_CONTROL2   0x0F

/**
 * @brief Initialize RX8025T RTC
 * 
 * @param i2c_bus I2C master bus handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rx8025t_init(void *i2c_bus);

/**
 * @brief Set RTC time
 * 
 * @param time Pointer to struct tm with time to set
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rx8025t_set_time(const struct tm *time);

/**
 * @brief Get RTC time
 * 
 * @param time Pointer to struct tm to store current time
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rx8025t_get_time(struct tm *time);

#ifdef __cplusplus
}
#endif
