#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "trend_manager.h"
#include "gauge_config.h" // For GasGaugeConfig generic references if needed

static const char *TAG = "TREND";

GaugeTrendData * all_trends = NULL;

#define STORAGE_BASE_PATH "/spiffs"
#define TREND_FILE_PATH "/spiffs/trends.bin"

esp_err_t trend_manager_init(void) {
    // 1. Mount SPIFFS
    esp_vfs_spiffs_conf_t conf = {
      .base_path = STORAGE_BASE_PATH,
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // 2. Allocate PSRAM
    all_trends = (GaugeTrendData *)heap_caps_malloc(sizeof(GaugeTrendData) * 16, MALLOC_CAP_SPIRAM);
    if (all_trends == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM for trends!");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize Memory (Clear or Load)
    memset(all_trends, 0, sizeof(GaugeTrendData) * 16);
    
    // 3. Load from File
    FILE* f = fopen(TREND_FILE_PATH, "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "No saved trends found. Starting fresh.");
    } else {
        size_t read_size = fread(all_trends, 1, sizeof(GaugeTrendData) * 16, f);
        fclose(f);
        ESP_LOGI(TAG, "Loaded %d bytes of trend data from storage", read_size);
    }
    
    return ESP_OK;
}

void trend_manager_add_point(int gauge_index, int value) {
    if (!all_trends || gauge_index < 0 || gauge_index >= 16) return;
    
    GaugeTrendData * t = &all_trends[gauge_index];
    
    t->data[t->head_index] = (uint16_t)value;
    t->head_index++;
    
    if (t->head_index >= TREND_HISTORY_SIZE) {
        t->head_index = 0;
        t->full_wrap = true;
    }
}

void trend_manager_save_to_nvs(void) {
    if (!all_trends) return;
    
    FILE* f = fopen(TREND_FILE_PATH, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open trend file for writing");
        return;
    }
    size_t written = fwrite(all_trends, 1, sizeof(GaugeTrendData) * 16, f);
    fclose(f);
    ESP_LOGI(TAG, "Saved %d bytes of trend data to storage", written);
}

// Helpers
uint16_t * trend_manager_get_data(int gauge_index) {
     if (!all_trends || gauge_index < 0 || gauge_index >= 16) return NULL;
     return all_trends[gauge_index].data;
}

uint16_t trend_manager_get_head(int gauge_index) {
    if (!all_trends || gauge_index < 0 || gauge_index >= 16) return 0;
    return all_trends[gauge_index].head_index;
}

bool trend_manager_is_full(int gauge_index) {
    if (!all_trends || gauge_index < 0 || gauge_index >= 16) return false;
    return all_trends[gauge_index].full_wrap;
}
