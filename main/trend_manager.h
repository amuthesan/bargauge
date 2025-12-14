#ifndef TREND_MANAGER_H
#define TREND_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define TREND_HISTORY_SIZE 1440 // 24 Hours * 60 Minutes

typedef struct {
    uint16_t data[TREND_HISTORY_SIZE];
    uint16_t head_index; // Points to the insertion point
    bool full_wrap;      // True if we have filled the buffer at least once
    // Timestamp of the last point? Or just assume 1 min interval.
    // For X-Axis, we just need relative time from "Now".
} GaugeTrendData;

// Global pointer to PSRAM data
extern GaugeTrendData * all_trends;

// Functions
esp_err_t trend_manager_init(void);
void trend_manager_add_point(int gauge_index, int value);
void trend_manager_save_to_nvs(void); // Actually saving to File (Storage partition)
void trend_manager_update_timer_cb(void * arg); // Timer callback
uint16_t * trend_manager_get_data(int gauge_index);
uint16_t trend_manager_get_head(int gauge_index);
bool trend_manager_is_full(int gauge_index);

#endif // TREND_MANAGER_H
