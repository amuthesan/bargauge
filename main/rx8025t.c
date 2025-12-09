/*
 * RX8025T-UC Real-Time Clock Driver Implementation
 */

#include "rx8025t.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>

static const char *TAG = "RX8025T";
static i2c_master_dev_handle_t rtc_dev_handle = NULL;

// BCD conversion helpers
static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

static uint8_t bcd_to_dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

esp_err_t rx8025t_init(void *i2c_bus) {
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Configure RTC device on I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RX8025T_I2C_ADDR,
        .scl_speed_hz = 400000, // 400kHz standard I2C
    };

    esp_err_t ret = i2c_master_bus_add_device((i2c_master_bus_handle_t)i2c_bus, &dev_cfg, &rtc_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add RTC device to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RX8025T RTC initialized");
    return ESP_OK;
}

esp_err_t rx8025t_set_time(const struct tm *time) {
    if (rtc_dev_handle == NULL) {
        ESP_LOGE(TAG, "RTC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (time == NULL) {
        ESP_LOGE(TAG, "Time pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Prepare time data in BCD format
    uint8_t time_data[7];
    time_data[0] = dec_to_bcd(time->tm_sec);        // Seconds (0-59)
    time_data[1] = dec_to_bcd(time->tm_min);        // Minutes (0-59)
    time_data[2] = dec_to_bcd(time->tm_hour);       // Hours (0-23)
    time_data[3] = dec_to_bcd(time->tm_wday);       // Weekday (0-6, Sun=0)
    time_data[4] = dec_to_bcd(time->tm_mday);       // Day (1-31)
    time_data[5] = dec_to_bcd(time->tm_mon + 1);    // Month (1-12, tm_mon is 0-11)
    time_data[6] = dec_to_bcd(time->tm_year % 100); // Year (00-99, tm_year is years since 1900)

    // Write time to RTC starting at seconds register
    uint8_t write_buf[8];
    write_buf[0] = RX8025T_REG_SECONDS; // Start register address
    memcpy(&write_buf[1], time_data, 7);

    esp_err_t ret = i2c_master_transmit(rtc_dev_handle, write_buf, 8, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write time to RTC: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RTC time set: %04d-%02d-%02d %02d:%02d:%02d",
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);

    return ESP_OK;
}

esp_err_t rx8025t_get_time(struct tm *time) {
    if (rtc_dev_handle == NULL) {
        ESP_LOGE(TAG, "RTC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (time == NULL) {
        ESP_LOGE(TAG, "Time pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg_addr = RX8025T_REG_SECONDS;
    uint8_t time_data[7];

    // Read 7 bytes starting from seconds register
    esp_err_t ret = i2c_master_transmit_receive(rtc_dev_handle, &reg_addr, 1, time_data, 7, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from RTC: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convert BCD to decimal
    time->tm_sec  = bcd_to_dec(time_data[0] & 0x7F); // Seconds (mask bit 7)
    time->tm_min  = bcd_to_dec(time_data[1] & 0x7F); // Minutes
    time->tm_hour = bcd_to_dec(time_data[2] & 0x3F); // Hours (mask bits 6-7)
    time->tm_wday = bcd_to_dec(time_data[3] & 0x07); // Weekday
    time->tm_mday = bcd_to_dec(time_data[4] & 0x3F); // Day
    time->tm_mon  = bcd_to_dec(time_data[5] & 0x1F) - 1; // Month (convert to 0-11)
    time->tm_year = bcd_to_dec(time_data[6]) + 100; // Year (add 100 for 2000-2099)

    // Not provided by RTC, set to defaults
    time->tm_isdst = -1; // Unknown DST

    ESP_LOGI(TAG, "RTC time read: %04d-%02d-%02d %02d:%02d:%02d",
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);

    return ESP_OK;
}
