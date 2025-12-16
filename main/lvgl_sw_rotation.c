#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "lvgl.h"
#include "esp_wifi_remote.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h" 
#include "modbus_master.h"
#include "mqtt_publisher.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "driver/spi_master.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_NONE // Force disable logging for Modbus silence
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_jd9365.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_cst816s.h"
#include "rx8025t.h" // RTC Driver
#include "trend_manager.h"
// #include "esp_lcd_touch_gsl3680.h" // Removed to avoid firmware duplication

#define ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS (0x40)
#define ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG()           \
    {                                       \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GSL3680_ADDRESS, \
        .control_phase_bytes = 1,           \
        .dc_bit_offset = 0,                 \
        .lcd_cmd_bits = 8,                 \
        .scl_speed_hz = 400000,            \
        .flags =                            \
        {                                   \
            .disable_control_phase = 1,     \
        }                                   \
    }

esp_err_t esp_lcd_touch_new_i2c_gsl3680(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch);
#include "lvgl_port_v9.h"
#include "lv_demos.h"
#include "driver/ppa.h"
#include <math.h>
#include <stdlib.h>

LV_FONT_DECLARE(lv_font_montserrat_36);
LV_FONT_DECLARE(lv_font_montserrat_48);
extern const lv_image_dsc_t logo_img;

static const char *TAG = "BarGauge";

// WiFi credentials (Global buffers with defaults)
char wifi_ssid[33] = "AKR Home";
char wifi_pass[65] = "brandy78755862";

// WiFi status
static bool wifi_connected = false;
static char system_ip_str[32] = "N/A"; // Defined before handler
// Duplicate time_label and callback removed


#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#define BSP_LCD_H_RES                       (800)
#define BSP_LCD_V_RES                       (1280)

#define BSP_I2C_NUM                         (I2C_NUM_1)
#define BSP_I2C_SDA                         (GPIO_NUM_7)
#define BSP_I2C_SCL                         (GPIO_NUM_8)

#define BSP_LCD_TOUCH_RST                   (GPIO_NUM_22)
#define BSP_LCD_TOUCH_INT                   (GPIO_NUM_21)

#define BSP_LCD_RST                         (GPIO_NUM_9)

i2c_master_bus_handle_t i2c_handle = NULL; 

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    
    // Update Global IP String
    snprintf(system_ip_str, sizeof(system_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
    
    wifi_connected = true;
    }
}

// NTP time sync callback
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized");
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Save to RTC when NTP syncs
    // Need to use localtime because RTC stores "wall clock" time without timezone info
    // But rx8025t_set_time expects struct tm
    if (rx8025t_set_time(&timeinfo) == ESP_OK) {
        ESP_LOGI(TAG, "RTC updated from NTP");
    } else {
        ESP_LOGE(TAG, "Failed to update RTC from NTP");
    }
} 

static const jd9365_lcd_init_cmd_t lcd_cmd[] = {
    {0xE0, (uint8_t[]){0x00}, 1, 0},
     {0xE1, (uint8_t[]){0x93}, 1, 0},
     {0xE2, (uint8_t[]){0x65}, 1, 0},
     {0xE3, (uint8_t[]){0xF8}, 1, 0},
     {0x80, (uint8_t[]){0x01}, 1, 0},
 
     {0xE0, (uint8_t[]){0x01}, 1, 0},
     {0x00, (uint8_t[]){0x00}, 1, 0},
     {0x01, (uint8_t[]){0x39}, 1, 0},
     {0x03, (uint8_t[]){0x10}, 1, 0},
     {0x04, (uint8_t[]){0x41}, 1, 0},
 
     {0x0C, (uint8_t[]){0x74}, 1, 0},
     {0x17, (uint8_t[]){0x00}, 1, 0},
     {0x18, (uint8_t[]){0xD7}, 1, 0},
     {0x19, (uint8_t[]){0x00}, 1, 0},
     {0x1A, (uint8_t[]){0x00}, 1, 0},
 
     {0x1B, (uint8_t[]){0xD7}, 1, 0},
     {0x1C, (uint8_t[]){0x00}, 1, 0},
     {0x24, (uint8_t[]){0xFE}, 1, 0},
     {0x35, (uint8_t[]){0x26}, 1, 0},
     {0x37, (uint8_t[]){0x69}, 1, 0},
 
     {0x38, (uint8_t[]){0x05}, 1, 0},
     {0x39, (uint8_t[]){0x06}, 1, 0},
     {0x3A, (uint8_t[]){0x08}, 1, 0},
     {0x3C, (uint8_t[]){0x78}, 1, 0},
     {0x3D, (uint8_t[]){0xFF}, 1, 0},
 
     {0x3E, (uint8_t[]){0xFF}, 1, 0},
     {0x3F, (uint8_t[]){0xFF}, 1, 0},
     {0x40, (uint8_t[]){0x06}, 1, 0},
     {0x41, (uint8_t[]){0xA0}, 1, 0},
     {0x43, (uint8_t[]){0x14}, 1, 0},
 
     {0x44, (uint8_t[]){0x0B}, 1, 0},
     {0x45, (uint8_t[]){0x30}, 1, 0},
     //{0x4A, (uint8_t[]){0x35}, 1, 0},//bist
     {0x4B, (uint8_t[]){0x04}, 1, 0},
     {0x55, (uint8_t[]){0x02}, 1, 0},
     {0x57, (uint8_t[]){0x89}, 1, 0},
 
     {0x59, (uint8_t[]){0x0A}, 1, 0},
     {0x5A, (uint8_t[]){0x28}, 1, 0},
 
     {0x5B, (uint8_t[]){0x15}, 1, 0},
     {0x5D, (uint8_t[]){0x50}, 1, 0},
     {0x5E, (uint8_t[]){0x37}, 1, 0},
     {0x5F, (uint8_t[]){0x29}, 1, 0},
     {0x60, (uint8_t[]){0x1E}, 1, 0},
 
     {0x61, (uint8_t[]){0x1D}, 1, 0},
     {0x62, (uint8_t[]){0x12}, 1, 0},
     {0x63, (uint8_t[]){0x1A}, 1, 0},
     {0x64, (uint8_t[]){0x08}, 1, 0},
     {0x65, (uint8_t[]){0x25}, 1, 0},
 
     {0x66, (uint8_t[]){0x26}, 1, 0},
     {0x67, (uint8_t[]){0x28}, 1, 0},
     {0x68, (uint8_t[]){0x49}, 1, 0},
     {0x69, (uint8_t[]){0x3A}, 1, 0},
     {0x6A, (uint8_t[]){0x43}, 1, 0},
 
     {0x6B, (uint8_t[]){0x3A}, 1, 0},
     {0x6C, (uint8_t[]){0x3B}, 1, 0},
     {0x6D, (uint8_t[]){0x32}, 1, 0},
     {0x6E, (uint8_t[]){0x1F}, 1, 0},
     {0x6F, (uint8_t[]){0x0E}, 1, 0},
 
     {0x70, (uint8_t[]){0x50}, 1, 0},
     {0x71, (uint8_t[]){0x37}, 1, 0},
     {0x72, (uint8_t[]){0x29}, 1, 0},
     {0x73, (uint8_t[]){0x1E}, 1, 0},
     {0x74, (uint8_t[]){0x1D}, 1, 0},
 
     {0x75, (uint8_t[]){0x12}, 1, 0},
     {0x76, (uint8_t[]){0x1A}, 1, 0},
     {0x77, (uint8_t[]){0x08}, 1, 0},
     {0x78, (uint8_t[]){0x25}, 1, 0},
     {0x79, (uint8_t[]){0x26}, 1, 0},
 
     {0x7A, (uint8_t[]){0x28}, 1, 0},
     {0x7B, (uint8_t[]){0x49}, 1, 0},
     {0x7C, (uint8_t[]){0x3A}, 1, 0},
     {0x7D, (uint8_t[]){0x43}, 1, 0},
     {0x7E, (uint8_t[]){0x3A}, 1, 0},
 
     {0x7F, (uint8_t[]){0x3B}, 1, 0},
     {0x80, (uint8_t[]){0x32}, 1, 0},
     {0x81, (uint8_t[]){0x1F}, 1, 0},
     {0x82, (uint8_t[]){0x0E}, 1, 0},
     {0xE0,(uint8_t []){0x02},1,0},
 
     {0x00,(uint8_t []){0x1F},1,0},
     {0x01,(uint8_t []){0x1F},1,0},
     {0x02,(uint8_t []){0x52},1,0},
     {0x03,(uint8_t []){0x51},1,0},
     {0x04,(uint8_t []){0x50},1,0},
 
     {0x05,(uint8_t []){0x4B},1,0},
     {0x06,(uint8_t []){0x4A},1,0},
     {0x07,(uint8_t []){0x49},1,0},
     {0x08,(uint8_t []){0x48},1,0},
     {0x09,(uint8_t []){0x47},1,0},
 
     {0x0A,(uint8_t []){0x46},1,0},
     {0x0B,(uint8_t []){0x45},1,0},
     {0x0C,(uint8_t []){0x44},1,0},
     {0x0D,(uint8_t []){0x40},1,0},
     {0x0E,(uint8_t []){0x41},1,0},
 
     {0x0F,(uint8_t []){0x1F},1,0},
     {0x10,(uint8_t []){0x1F},1,0},
     {0x11,(uint8_t []){0x1F},1,0},
     {0x12,(uint8_t []){0x1F},1,0},
     {0x13,(uint8_t []){0x1F},1,0},
 
     {0x14,(uint8_t []){0x1F},1,0},
     {0x15,(uint8_t []){0x1F},1,0},
     {0x16,(uint8_t []){0x1F},1,0},
     {0x17,(uint8_t []){0x1F},1,0},
     {0x18,(uint8_t []){0x52},1,0},
 
     {0x19,(uint8_t []){0x51},1,0},
     {0x1A,(uint8_t []){0x50},1,0},
     {0x1B,(uint8_t []){0x4B},1,0},
     {0x1C,(uint8_t []){0x4A},1,0},
     {0x1D,(uint8_t []){0x49},1,0},
 
     {0x1E,(uint8_t []){0x48},1,0},
     {0x1F,(uint8_t []){0x47},1,0},
     {0x20,(uint8_t []){0x46},1,0},
     {0x21,(uint8_t []){0x45},1,0},
     {0x22,(uint8_t []){0x44},1,0},
 
     {0x23,(uint8_t []){0x40},1,0},
     {0x24,(uint8_t []){0x41},1,0},
     {0x25,(uint8_t []){0x1F},1,0},
     {0x26,(uint8_t []){0x1F},1,0},
     {0x27,(uint8_t []){0x1F},1,0},
 
     {0x28,(uint8_t []){0x1F},1,0},
     {0x29,(uint8_t []){0x1F},1,0},
     {0x2A,(uint8_t []){0x1F},1,0},
     {0x2B,(uint8_t []){0x1F},1,0},
     {0x2C,(uint8_t []){0x1F},1,0},
 
     {0x2D,(uint8_t []){0x1F},1,0},
     {0x2E,(uint8_t []){0x52},1,0},
     {0x2F,(uint8_t []){0x40},1,0},
     {0x30,(uint8_t []){0x41},1,0},
     {0x31,(uint8_t []){0x48},1,0},
 
     {0x32,(uint8_t []){0x49},1,0},
     {0x33,(uint8_t []){0x4A},1,0},
     {0x34,(uint8_t []){0x4B},1,0},
     {0x35,(uint8_t []){0x44},1,0},
     {0x36,(uint8_t []){0x45},1,0},
 
     {0x37,(uint8_t []){0x46},1,0},
     {0x38,(uint8_t []){0x47},1,0},
     {0x39,(uint8_t []){0x51},1,0},
     {0x3A,(uint8_t []){0x50},1,0},
     {0x3B,(uint8_t []){0x1F},1,0},
 
     {0x3C,(uint8_t []){0x1F},1,0},
     {0x3D,(uint8_t []){0x1F},1,0},
     {0x3E,(uint8_t []){0x1F},1,0},
     {0x3F,(uint8_t []){0x1F},1,0},
     {0x40,(uint8_t []){0x1F},1,0},
 
     {0x41,(uint8_t []){0x1F},1,0},
     {0x42,(uint8_t []){0x1F},1,0},
     {0x43,(uint8_t []){0x1F},1,0},
     {0x44,(uint8_t []){0x52},1,0},
     {0x45,(uint8_t []){0x40},1,0},
 
     {0x46,(uint8_t []){0x41},1,0},
     {0x47,(uint8_t []){0x48},1,0},
     {0x48,(uint8_t []){0x49},1,0},
     {0x49,(uint8_t []){0x4A},1,0},
     {0x4A,(uint8_t []){0x4B},1,0},
 
     {0x4B,(uint8_t []){0x44},1,0},
     {0x4C,(uint8_t []){0x45},1,0},
     {0x4D,(uint8_t []){0x46},1,0},
     {0x4E,(uint8_t []){0x47},1,0},
     {0x4F,(uint8_t []){0x51},1,0},
 
     {0x50,(uint8_t []){0x50},1,0},
     {0x51,(uint8_t []){0x1F},1,0},
     {0x52,(uint8_t []){0x1F},1,0},
     {0x53,(uint8_t []){0x1F},1,0},
     {0x54,(uint8_t []){0x1F},1,0},
 
     {0x55,(uint8_t []){0x1F},1,0},
     {0x56,(uint8_t []){0x1F},1,0},
     {0x57,(uint8_t []){0x1F},1,0},
     {0x58,(uint8_t []){0x40},1,0},
     {0x59,(uint8_t []){0x00},1,0},
 
     {0x5A,(uint8_t []){0x00},1,0},
     {0x5B,(uint8_t []){0x10},1,0},
     {0x5C,(uint8_t []){0x05},1,0},
     {0x5D,(uint8_t []){0x50},1,0},
     {0x5E,(uint8_t []){0x01},1,0},
 
     {0x5F,(uint8_t []){0x02},1,0},
     {0x60,(uint8_t []){0x50},1,0},
     {0x61,(uint8_t []){0x06},1,0},
     {0x62,(uint8_t []){0x04},1,0},
     {0x63,(uint8_t []){0x03},1,0},
 
     {0x64,(uint8_t []){0x64},1,0},
     {0x65,(uint8_t []){0x65},1,0},
     {0x66,(uint8_t []){0x0B},1,0},
     {0x67,(uint8_t []){0x73},1,0},
     {0x68,(uint8_t []){0x07},1,0},
 
     {0x69,(uint8_t []){0x06},1,0},
     {0x6A,(uint8_t []){0x64},1,0},
     {0x6B,(uint8_t []){0x08},1,0},
     {0x6C,(uint8_t []){0x00},1,0},
     {0x6D,(uint8_t []){0x32},1,0},
 
     {0x6E,(uint8_t []){0x08},1,0},
     {0xE0,(uint8_t []){0x04},1,0},
     {0x2C,(uint8_t []){0x6B},1,0},
     {0x35,(uint8_t []){0x08},1,0},
     {0x37,(uint8_t []){0x00},1,0},
 
     {0xE0,(uint8_t []){0x00},1,0},
     {0x11,(uint8_t []){0x00},1,0},
     {0x29, (uint8_t[]){0x00}, 1, 5},
     {0x11, (uint8_t[]){0x00}, 1, 120},
     {0x35, (uint8_t[]){0x00}, 1, 0},
 
};

IRAM_ATTR static bool mipi_dsi_lcd_on_vsync_event(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    return lvgl_port_notify_lcd_vsync();
}

#define BSP_LCD_BACKLIGHT   GPIO_NUM_23
#define LCD_LEDC_CH         LEDC_CHANNEL_0
static esp_err_t bsp_display_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

static esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    return ESP_OK;
}

// static esp_err_t bsp_display_backlight_off(void)
// {
//     return bsp_display_brightness_set(0);
// }

static esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

#include "gauge_config.h"

static GasGaugeConfig gauge_configs[16];
static SafetyConfig safety_config; // Global Instance
static ServiceConfig service_config; // Global Service Config

static void mqtt_timer_cb(lv_timer_t * t) {
    float values[16];
    for(int i=0; i<16; i++) { // Processing all 16 channels
        int raw_val = sys_modbus_data.analog_vals[i];
        long in_min = gauge_configs[i].analog_min;
        long in_max = gauge_configs[i].analog_max;
        long out_min = gauge_configs[i].min_val;
        long out_max = gauge_configs[i].max_val;
        float val = (float)raw_val; 
        if ((in_max - in_min) != 0) {
             val = (float)((raw_val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
        }
        values[i] = val;
    }
    mqtt_publish_gauge_data(values, gauge_configs, 16);
}

static int current_edit_index = 0; // Index of gauge currently being edited in settings
static int current_page = 0; // 0 for Page 1, 1 for Page 2

// Gauge Arrays - Expanded to 16
static lv_obj_t * gauge_widgets[16] = {NULL};
static lv_obj_t * gauge_arcs[16] = {NULL};
static lv_obj_t * gauge_labels[16] = {NULL}; // Digital Value
static lv_obj_t * gauge_title_labels[16] = {NULL};
static lv_obj_t * gauge_unit_labels[16] = {NULL};
static lv_obj_t * gauge_status_labels[16] = {NULL}; // SAFE/WARNING Label

static lv_obj_t * mb_status_label = NULL; // Modbus Status Label
static lv_obj_t * trending_screen = NULL;
static lv_obj_t * trending_title_label = NULL;
// static lv_obj_t * const_chart = NULL; // Moved to near usage
// static lv_chart_series_t * const_ser1 = NULL; // Moved to near usage
static lv_obj_t * warning_label = NULL; // Status Warning Label (Top Left)
static lv_obj_t * relay_leds[16] = {NULL};
static lv_obj_t * input_leds[4] = {NULL};

static bool alarm_acknowledged = false; // Acknowledge State

static lv_obj_t * grid_page_1 = NULL;
static lv_obj_t * grid_page_2 = NULL;
static lv_obj_t * grid_page_3 = NULL; // Relay Screen
static lv_obj_t * btn_next = NULL;
static lv_obj_t * btn_prev = NULL;

// Warning Page Objects
static lv_obj_t * warning_screen = NULL;
static lv_obj_t * last_active_screen = NULL; // To return after Ack

// Service Page Objects
static lv_obj_t * service_screen = NULL;
static lv_obj_t * reminder_screen = NULL;

// --- Global State ---
static uint16_t gauge_active_mask = 0xFFFF; // Bitmask for active gauges (Default: All On)

// Save Configs to NVS
// Save Configs to NVS
void save_gauge_configs(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;

    nvs_set_u16(my_handle, "gauge_mask", gauge_active_mask); // Save Active Mask

    // Save each gauge config as a BLOB
    for(int i=0; i<16; i++) {
        char key[16];
        snprintf(key, sizeof(key), "g_blob_%d", i);
        nvs_set_blob(my_handle, key, &gauge_configs[i], sizeof(GasGaugeConfig));
    }
    
    nvs_set_blob(my_handle, "safety_cfg", &safety_config, sizeof(safety_config)); // Save Safety Config
    nvs_set_blob(my_handle, "srv_cfg", &service_config, sizeof(service_config)); // Save Service Config
    
    nvs_commit(my_handle);
    nvs_close(my_handle);
    ESP_LOGI(TAG, "Gauge & Safety & Service Configs Saved to NVS (BLOB Format)");
}

// Load Configs from NVS
void load_gauge_configs(void) {
    // Set initial defaults for all gauges and safety config
    for(int i=0; i<16; i++) {
        snprintf(gauge_configs[i].name, 32, "GAUGE %d", i+1);
        snprintf(gauge_configs[i].unit, 16, "PPM");
        gauge_configs[i].min_val = 0;
        gauge_configs[i].max_val = 100;
        gauge_configs[i].blue_limit = 30;
        gauge_configs[i].yellow_limit = 70;
        gauge_configs[i].red_limit = 100;
        gauge_configs[i].threshold = 0; // Default off
        gauge_configs[i].analog_min = 4000;
        gauge_configs[i].analog_max = 20000;
        gauge_configs[i].trigger_relay_index = 0; // Default None
    }
    // Safety Defaults
    safety_config.siren_relay_index = 0; 
    safety_config.siren_invert = false;
    safety_config.strobe_relay_index = 0; 
    safety_config.strobe_invert = false;
    
    // Service Defaults
    service_config.cal_year = 2024;
    service_config.cal_month = 1;
    service_config.cal_day = 1;
    service_config.expiry_period = 1; // Default 1 Year
    service_config.last_ack_year = 0;

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS Open failed (%s). Using Defaults.", esp_err_to_name(err));
        return;
    }

    // Load Active Mask
    uint16_t mask_val = 0xFFFF;
    if(nvs_get_u16(my_handle, "gauge_mask", &mask_val) == ESP_OK) {
        gauge_active_mask = mask_val;
    }

    // Load Blobs
    for(int i=0; i<16; i++) {
        char key[16];
        snprintf(key, sizeof(key), "g_blob_%d", i);
        size_t required_size = sizeof(GasGaugeConfig);
        err = nvs_get_blob(my_handle, key, &gauge_configs[i], &required_size);
        
        if (err == ESP_OK) {
            ESP_LOGD(TAG, "Loaded Blob for Gauge %d", i);
        } else {
             ESP_LOGD(TAG, "Gauge %d Not Found (Using Defaults)", i);
        }
    }
    
    // Load Safety Config
    size_t required_size = sizeof(safety_config);
    nvs_get_blob(my_handle, "safety_cfg", &safety_config, &required_size);
    
    required_size = sizeof(service_config);
    nvs_get_blob(my_handle, "srv_cfg", &service_config, &required_size);

    nvs_close(my_handle);
    ESP_LOGI(TAG, "Gauge Configs Loaded (BLOB Format).");
}

// Save WiFi Credentials to NVS
void save_wifi_creds(const char* ssid, const char* pass) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
    nvs_set_str(my_handle, "wifi_ssid", ssid);
    nvs_set_str(my_handle, "wifi_pass", pass);
    nvs_commit(my_handle);
    nvs_close(my_handle);
    ESP_LOGI(TAG, "WiFi Credentials Saved: %s", ssid);
}

// Load WiFi Credentials from NVS
void load_wifi_creds(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;
    
    size_t required_size = sizeof(wifi_ssid);
    if(nvs_get_str(my_handle, "wifi_ssid", wifi_ssid, &required_size) == ESP_OK) {
         ESP_LOGI(TAG, "Loaded SSID from NVS: %s", wifi_ssid);
    }
    
    required_size = sizeof(wifi_pass);
    if(nvs_get_str(my_handle, "wifi_pass", wifi_pass, &required_size) == ESP_OK) {
         ESP_LOGI(TAG, "Loaded Password from NVS");
    }
    nvs_close(my_handle);
}

// Forward declarations
static lv_obj_t * create_gas_widget(lv_obj_t *parent, int index);
static void create_reminder_screen(bool is_preview);
static void create_settings_screen(void);
static void create_main_screen(void);
// static void create_splash_screen(void); // Removed
static void create_service_screen(void);
// static void create_reminder_screen(bool is_preview); // Duplicate removed
static void next_page_cb(lv_event_t * e);
static void prev_page_cb(lv_event_t * e);
static void back_from_trending_cb(lv_event_t * e);

// Global/Static references for updates
static lv_obj_t * time_label = NULL; // For time display
static lv_obj_t * wifi_status_icon = NULL; // For WiFi status

static lv_obj_t * main_screen = NULL;
static lv_obj_t * settings_screen = NULL;
static lv_obj_t * kb = NULL; // Global keyboard

// --- Trending Checkbox Callback ---
static int current_trending_index = 0;
static bool trending_live_mode = false;
static lv_obj_t * const_chart = NULL;
static lv_chart_series_t * const_ser1 = NULL;
static lv_obj_t * trend_x_labels[5] = {NULL}; // For X-axis time marks
static lv_obj_t * y_max_label = NULL;
static lv_obj_t * y_min_label = NULL;

static void refresh_trending_chart(void) {
    if (!const_chart || !const_ser1) return;

    // Update Title
    if (trending_title_label) {
        lv_label_set_text_fmt(trending_title_label, "Trending Data for %s (%s)", 
            gauge_configs[current_trending_index].name,
            trending_live_mode ? "LIVE" : "24 Hours");
    }

    lv_chart_set_range(const_chart, LV_CHART_AXIS_PRIMARY_Y, 
        gauge_configs[current_trending_index].min_val, 
        gauge_configs[current_trending_index].max_val);
    
    // Update Y-Axis Labels
    if(y_max_label) lv_label_set_text_fmt(y_max_label, "%d", gauge_configs[current_trending_index].max_val);
    if(y_min_label) lv_label_set_text_fmt(y_min_label, "%d", gauge_configs[current_trending_index].min_val);

    // Update X-Axis Labels (Time)
    time_t now;
    time(&now);
    struct tm timeinfo;
    
    // In Live Mode, labels are different or static "Now"
    // In 24h Mode, labels are -24h to Now
    for(int i=0; i<5; i++) {
        if(trend_x_labels[i]) {
            if(trending_live_mode) {
                // Live Mode Labels
                if(i == 4) lv_label_set_text(trend_x_labels[i], "Now");
                else if(i == 0) lv_label_set_text(trend_x_labels[i], "-10s");
                else lv_label_set_text(trend_x_labels[i], ""); // Hide others
            } else {
                // 24h Mode Labels
                // Marks: -24h, -18h, -12h, -6h, Now (Index 0 to 4)
                // Actually let's do: 0 (-24), 1 (-18), 2 (-12), 3 (-6), 4 (Now)
                int offset_hours = (4 - i) * 6; // 24, 18, 12, 6, 0
                time_t past = now - (offset_hours * 3600);
                localtime_r(&past, &timeinfo);
                
                lv_label_set_text_fmt(trend_x_labels[i], "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            }
        }
    }

    if (trending_live_mode) {
        // Live Mode: Reset to empty or small window, relying on timer to push data
        lv_chart_set_point_count(const_chart, 50); // 50 points window
        // lv_chart_refresh(const_chart); // Will be filled by timer
    } else {
        // 24h Mode: Load from Trend Manager
        lv_chart_set_point_count(const_chart, TREND_HISTORY_SIZE);
        
        uint16_t * raw_data = trend_manager_get_data(current_trending_index);
        uint16_t head = trend_manager_get_head(current_trending_index);
        bool full = trend_manager_is_full(current_trending_index);
        
        if (raw_data) {
             lv_chart_series_t * ser = const_ser1;
             
             // Calculate Count
             int count = full ? TREND_HISTORY_SIZE : head; 
             if (count == 0) count = 1; // Avoid 0 count
             
             // Resize (Temporary, LVGL might realloc, ensure this is safe)
             // Actually lv_chart_set_point_count does realloc.
             // If we set count to 1440, we MUST provide 1440 points or it looks weird?
             // No, standard is 1440. We shift data in.
             
             // Wait, if not full, we only have 'head' points. 
             // If we set point_count=1440, 0s will be displayed?
             // Better to set point_count = count.
             lv_chart_set_point_count(const_chart, count);

             for(int i=0; i < count; i++) {
                 int idx;
                 if (full) {
                     idx = (head + i) % TREND_HISTORY_SIZE;
                 } else {
                     idx = i;
                 }
                 
                 // Raw Value -> Scaled Value? 
                 // Chart range is already set to min/max.
                 // But raw_data is RAW analog (4000-20000). 
                 // We need to map it to Gauge Values (0-100 etc) BEFORE putting in chart.
                 
                 int raw = raw_data[idx];
                 long in_min = gauge_configs[current_trending_index].analog_min;
                 long in_max = gauge_configs[current_trending_index].analog_max;
                 long out_min = gauge_configs[current_trending_index].min_val;
                 long out_max = gauge_configs[current_trending_index].max_val;
                 
                 int val = raw;
                 if ((in_max - in_min) != 0) {
                      val = (int)((raw - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
                 }
                 
                 lv_chart_set_next_value(const_chart, ser, val);
             }
        }
    }
}

static void trending_mode_sw_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    trending_live_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    refresh_trending_chart();
}

static void create_trending_screen(void); // Forward declaration

static void trending_btn_event_cb(lv_event_t * e) {
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    current_trending_index = (int)idx;
    
    // Ensure screen is created
    if (!trending_screen) {
        create_trending_screen();
    }
    
    // Reset to 24h mode by default? Or keep state? Let's keep 24h default.
    // If we want persistent state, don't reset. But user asked for option.
    // Let's reset purely for predictability or check switch state.
    // If screen was just created, switch is OFF (24h).
    // If screen existed, we should sync with switch.
    // Let's force refresh based on current switch state (which might be preserved).
    
    refresh_trending_chart();

    lv_scr_load(trending_screen);
    ESP_LOGI(TAG, "Opened Trending for Gauge %d", current_trending_index + 1);
}

static void create_trending_screen(void) {
    if(trending_screen) return; // Already created

    trending_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(trending_screen, lv_color_hex(0x000000), 0); // Black bg

    trending_title_label = lv_label_create(trending_screen);
    lv_label_set_text(trending_title_label, "Trending Data"); // Updated by refresh
    lv_obj_set_style_text_font(trending_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(trending_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(trending_title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Chart
    lv_obj_t * chart = lv_chart_create(trending_screen);
    lv_obj_set_size(chart, 1100, 600);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, TREND_HISTORY_SIZE); 
    lv_chart_set_div_line_count(chart, 5, 7); 
    
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x101010), 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x404040), 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x303030), LV_PART_MAIN); 
    
    // Enable Axis Ticks - Removed for LVGL v9 compatibility
    // lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 40);
    // lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 20);

    const_ser1 = lv_chart_add_series(chart, lv_color_hex(0x00E0FF), LV_CHART_AXIS_PRIMARY_Y);
    const_chart = chart;

    // Y-Axis Labels (Manual)
    y_max_label = lv_label_create(trending_screen);
    lv_obj_set_style_text_font(y_max_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(y_max_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align_to(y_max_label, chart, LV_ALIGN_OUT_LEFT_TOP, -10, 0);
    lv_label_set_text(y_max_label, "100");

    y_min_label = lv_label_create(trending_screen);
    lv_obj_set_style_text_font(y_min_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(y_min_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align_to(y_min_label, chart, LV_ALIGN_OUT_LEFT_BOTTOM, -10, 0);
    lv_label_set_text(y_min_label, "0");

    // X-Axis Label Container (Below Chart)
    lv_obj_t * x_axis_cont = lv_obj_create(trending_screen);
    lv_obj_set_size(x_axis_cont, 1100, 30);
    lv_obj_align_to(x_axis_cont, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(x_axis_cont, 0, 0); // Transparent
    lv_obj_set_style_border_width(x_axis_cont, 0, 0);
    lv_obj_set_flex_flow(x_axis_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(x_axis_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Create 5 Static Labels
    for(int i=0; i<5; i++) {
        trend_x_labels[i] = lv_label_create(x_axis_cont);
        lv_label_set_text(trend_x_labels[i], "--:--");
        lv_obj_set_style_text_color(trend_x_labels[i], lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(trend_x_labels[i], &lv_font_montserrat_16, 0);
    }

    // Toggle: Live vs 24h
    lv_obj_t * sw_mode = lv_switch_create(trending_screen);
    lv_obj_align(sw_mode, LV_ALIGN_TOP_RIGHT, -40, 20);
    lv_obj_add_event_cb(sw_mode, trending_mode_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * lbl_sw = lv_label_create(trending_screen);
    lv_label_set_text(lbl_sw, "Live Mode");
    lv_obj_set_style_text_color(lbl_sw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(lbl_sw, sw_mode, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    // Back Button
    lv_obj_t * btn_back = lv_btn_create(trending_screen);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 40, 20);
    lv_obj_add_event_cb(btn_back, back_from_trending_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "BACK");
    lv_obj_center(lbl_back);
}

// Settings Text Areas
static lv_obj_t * ta_name = NULL;
static lv_obj_t * ta_unit = NULL;
static lv_obj_t * ta_min = NULL;
static lv_obj_t * ta_max = NULL;
static lv_obj_t * ta_blue = NULL;
static lv_obj_t * ta_yellow = NULL;
static lv_obj_t * ta_analog_min = NULL;
static lv_obj_t * ta_analog_max = NULL;
static lv_obj_t * ta_threshold = NULL;
static lv_obj_t * dd_trigger = NULL; // Dropdown for Independent Trigger

static lv_obj_t * sys_wifi_label = NULL;
static lv_obj_t * sys_ip_label = NULL;
static lv_obj_t * sys_mqtt_label = NULL; // MQTT Status

// WiFi Config UI Handles
static lv_obj_t * ta_ssid = NULL;
static lv_obj_t * ta_pass = NULL;
static lv_obj_t * lbl_wifi_status = NULL; // On Tab 5


// --- Activation Checkbox Callback ---
static void activation_checkbox_cb(lv_event_t * e) {
    lv_obj_t * cb = lv_event_get_target(e);
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    bool active = lv_obj_has_state(cb, LV_STATE_CHECKED);
    
    if(active) {
        gauge_active_mask |= (1 << idx);
    } else {
        gauge_active_mask &= ~(1 << idx);
    }
    
    // Auto-save on toggle
    save_gauge_configs();
    ESP_LOGI(TAG, "Gauge %d Activation Changed: %d. Mask: 0x%04X", (int)idx + 1, active, gauge_active_mask);
}

void settings_btn_event_cb(lv_event_t * e) {
    // Get the index of the gauge that triggered this event
    current_edit_index = (int)lv_event_get_user_data(e);
    if (settings_screen == NULL) {
        create_settings_screen();
    }
    lv_scr_load_anim(settings_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0, false);
}

static void back_from_trending_cb(lv_event_t * e) {
    if (main_screen) {
        lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, false);
    }
}

static void back_from_settings_cb(lv_event_t * e) {
    if (main_screen) {
        // Update Title if changed
        if (gauge_title_labels[current_edit_index]) {
            lv_label_set_text(gauge_title_labels[current_edit_index], gauge_configs[current_edit_index].name);
        }
        if (gauge_unit_labels[current_edit_index]) {
            lv_label_set_text(gauge_unit_labels[current_edit_index], gauge_configs[current_edit_index].unit);
        }
        
        // Update Arc Range dynamically
        if (gauge_arcs[current_edit_index]) {
            lv_arc_set_range(gauge_arcs[current_edit_index], gauge_configs[current_edit_index].min_val, gauge_configs[current_edit_index].max_val);
        }
        
        // Save to NVS
        // save_gauge_configs(); // REMOVED: Manual Save Only now
        
        lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 500, 0, false);
    }
}

// --- Keyboard Event Handlers ---
static void ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        // Focus keyboard on this text area
        if(kb != NULL) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // Set keyboard mode based on content
            // Name field -> Text mode
            // Others -> Number mode
            // We can differentiate by User Data passed
            long mode = (long)lv_event_get_user_data(e);
            if (mode == 1) { // Name or Unit
                lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
            } else { // Numbers
                lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
            }
        }
    } else if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// --------------------------------------------------------------------------
//                         WARNING SCREEN
// --------------------------------------------------------------------------
static lv_obj_t * lbl_warning_source = NULL;

static void perform_acknowledge(void) {
    if (!alarm_acknowledged) {
        alarm_acknowledged = true;
        ESP_LOGI(TAG, "Alarm Acknowledged via Hardware Button");
        
        // Return to previous screen
        if (last_active_screen) {
            lv_scr_load_anim(last_active_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
        } else {
            lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
        }
    }
}

// --- Warning Icon Custom Draw ---
static void warning_icon_draw_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);
    
    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);
    int32_t x = obj_coords.x1;
    int32_t y = obj_coords.y1;

    // 1. Draw Yellow Triangle
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = lv_color_hex(0xFFD700);
    tri_dsc.opa = LV_OPA_COVER;
    
    // Points: Top, Bottom-Right, Bottom-Left
    int32_t pad = 10;
    tri_dsc.p[0].x = x + w / 2;
    tri_dsc.p[0].y = y + pad;
    
    tri_dsc.p[1].x = x + w - pad;
    tri_dsc.p[1].y = y + h - pad;
    
    tri_dsc.p[2].x = x + pad;
    tri_dsc.p[2].y = y + h - pad;

    lv_draw_triangle(layer, &tri_dsc);

    // 2. Draw Exclamation Mark "!"
    // We can draw a thick line and a dot, or use text. Text is easier if font is good.
    // Let's use a simple rounded rect for the stick and a circle for the dot for pure procedural perfection.
    
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x000000); // Black
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    
    // Stick
    int32_t stick_w = 20;
    int32_t stick_h = 80;
    lv_area_t stick_area;
    stick_area.x1 = x + (w - stick_w) / 2;
    stick_area.y1 = y + 60;
    stick_area.x2 = stick_area.x1 + stick_w;
    stick_area.y2 = stick_area.y1 + stick_h;
    lv_draw_rect(layer, &rect_dsc, &stick_area);
    
    // Dot
    int32_t dot_size = 22;
    lv_area_t dot_area;
    dot_area.x1 = x + (w - dot_size) / 2;
    dot_area.y1 = stick_area.y2 + 15;
    dot_area.x2 = dot_area.x1 + dot_size;
    dot_area.y2 = dot_area.y1 + dot_size;
    lv_draw_rect(layer, &rect_dsc, &dot_area);
}

static void create_warning_screen(void) {
    warning_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(warning_screen, lv_color_hex(0x800000), 0); // Deep Red
    lv_obj_set_flex_flow(warning_screen, LV_FLEX_FLOW_COLUMN);
    // Align START (Top) to control vertical spacing
    lv_obj_set_flex_align(warning_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(warning_screen, 40, 0); // Icon High Up
    
    // Icon Container (Fixes alignment/leaning) & Custom Draw
    lv_obj_t * icon_cont = lv_obj_create(warning_screen);
    lv_obj_set_size(icon_cont, 250, 200); // Box to hold the icon
    lv_obj_set_style_bg_opa(icon_cont, 0, 0); // Transparent
    lv_obj_set_style_border_width(icon_cont, 0, 0);
    lv_obj_set_style_margin_bottom(icon_cont, 100, 0); // Push content down
    
    // Attach Custom Draw Callback
    lv_obj_add_event_cb(icon_cont, warning_icon_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
    // Force a redraw to ensure it appears
    lv_obj_invalidate(icon_cont);
    
    // Source Label (Dynamic)
    lbl_warning_source = lv_label_create(warning_screen);
    lv_label_set_text(lbl_warning_source, "Source: Unknown\nValue: ---");
    lv_obj_set_style_text_font(lbl_warning_source, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_warning_source, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(lbl_warning_source, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(lbl_warning_source, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lbl_warning_source, LV_OPA_50, 0); // Semi-transparent black box
    lv_obj_set_style_pad_all(lbl_warning_source, 10, 0);
    lv_obj_set_style_radius(lbl_warning_source, 10, 0);
    lv_obj_set_style_margin_top(lbl_warning_source, 20, 0);
    lv_obj_set_style_margin_bottom(lbl_warning_source, 20, 0);

    // Generic Message
    lv_obj_t * msg = lv_label_create(warning_screen);
    lv_label_set_text(msg, "Sensor Threshold Exceeded.");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    
    // Instruction
    lv_obj_t * instr = lv_label_create(warning_screen);
    lv_label_set_text(instr, "Press Alarm Acknowledge Button\nto Acknowledge");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_20, 0); // Smaller
    lv_obj_set_style_text_color(instr, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(instr, 40, 0);
}

// --------------------------------------------------------------------------
//                         SETTINGS SCREEN
// --------------------------------------------------------------------------

static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * k = lv_event_get_target(e);
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
       lv_keyboard_set_textarea(k, NULL);
       lv_obj_add_flag(k, LV_OBJ_FLAG_HIDDEN);
    }
}

// --- Value Update Handlers ---
static void name_ta_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_DEFOCUSED) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(ta);
        snprintf(gauge_configs[current_edit_index].name, sizeof(gauge_configs[current_edit_index].name), "%s", txt);
    }
    ta_event_cb(e); // Chain to standard handler
}

static void unit_ta_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_DEFOCUSED) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(ta);
        snprintf(gauge_configs[current_edit_index].unit, sizeof(gauge_configs[current_edit_index].unit), "%s", txt);
    }
    ta_event_cb(e); // Chain to standard handler
}

static void int_val_ta_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_DEFOCUSED) {
        lv_obj_t * ta = lv_event_get_target(e);
        int field_id = (int)lv_event_get_user_data(e);
        const char * txt = lv_textarea_get_text(ta);
        int val = atoi(txt);
        
        if(field_id == 10) gauge_configs[current_edit_index].min_val = val;
        else if(field_id == 11) gauge_configs[current_edit_index].max_val = val;
        else if(field_id == 12) gauge_configs[current_edit_index].blue_limit = val;
        else if(field_id == 13) gauge_configs[current_edit_index].yellow_limit = val;
        else if(field_id == 14) gauge_configs[current_edit_index].analog_min = val;
        else if(field_id == 12) gauge_configs[current_edit_index].blue_limit = val;
        else if(field_id == 13) gauge_configs[current_edit_index].yellow_limit = val;
        else if(field_id == 14) gauge_configs[current_edit_index].analog_min = val;
        else if(field_id == 15) gauge_configs[current_edit_index].analog_max = val;
        else if(field_id == 16) gauge_configs[current_edit_index].threshold = val;
    }
    
    // Manual Keyboard Logic
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        lv_obj_t * ta = lv_event_get_target(e);
        if(kb) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
        }
    } else if(code == LV_EVENT_DEFOCUSED) {
        if(kb) {
             lv_keyboard_set_textarea(kb, NULL);
             lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Helper to create valid TA row
static lv_obj_t * create_config_row(lv_obj_t * parent, const char * title, const char * text_buffer, int id, int type) {
    // Type 0: Int, 1: Name, 2: Unit
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 780, 45); // Ultra-Compact row height
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0); // Zero padding
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0); // Smaller font
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t * ta = lv_textarea_create(cont);
    lv_obj_set_size(ta, 200, 35); // Smaller, wider aspect ratio
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_16, 0); // Match label font
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, text_buffer); // Direct value set
    
    if (type == 1) { // Name
        lv_textarea_set_max_length(ta, 31);
        lv_obj_add_event_cb(ta, name_ta_cb, LV_EVENT_ALL, (void*)1); 
    } else if (type == 2) { // Unit
        lv_textarea_set_max_length(ta, 15);
        lv_obj_add_event_cb(ta, unit_ta_cb, LV_EVENT_ALL, (void*)1); 
    } else { // Int
        lv_textarea_set_max_length(ta, 5); // Max 5 digits
        // Pass int ID as User Data
        lv_obj_add_event_cb(ta, int_val_ta_cb, LV_EVENT_ALL, (void*)id); 
    }
    return ta;
}

static void dd_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        current_edit_index = lv_dropdown_get_selected(obj);
        
        // Refresh all TAs with new config values
        lv_textarea_set_text(ta_name, gauge_configs[current_edit_index].name);
        lv_textarea_set_text(ta_unit, gauge_configs[current_edit_index].unit);
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].min_val);
        lv_textarea_set_text(ta_min, buf);
        
        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].max_val);
        lv_textarea_set_text(ta_max, buf);
        
        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].blue_limit);
        lv_textarea_set_text(ta_blue, buf);
        
        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].yellow_limit);
        lv_textarea_set_text(ta_yellow, buf);

        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].analog_min);
        lv_textarea_set_text(ta_analog_min, buf);

        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].analog_max);
        lv_textarea_set_text(ta_analog_max, buf);

        snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].threshold);
        lv_textarea_set_text(ta_threshold, buf);
        
        if(dd_trigger) {
             lv_dropdown_set_selected(dd_trigger, gauge_configs[current_edit_index].trigger_relay_index);
        }
    }
}

static void trigger_dd_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    gauge_configs[current_edit_index].trigger_relay_index = lv_dropdown_get_selected(dd);
}

static void siren_dd_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    safety_config.siren_relay_index = lv_dropdown_get_selected(dd);
}
static void siren_sw_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    safety_config.siren_invert = lv_obj_has_state(sw, LV_STATE_CHECKED);
}
static void strobe_dd_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    safety_config.strobe_relay_index = lv_dropdown_get_selected(dd);
}
static void strobe_sw_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    safety_config.strobe_invert = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

// --- WiFi Callbacks ---
static void save_config_btn_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    
    // Trigger Save
    save_gauge_configs(); // Saves ALL configs to NVS
    
    // Visual Feedback
    lv_label_set_text(label, "SAVED!");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), 0); // Green
    
    // Revert after delay? Or just leave it. Leaving it is fine until clicked again or screen reload.
}

static void wifi_connect_btn_cb(lv_event_t * e) {
    const char * ssid = lv_textarea_get_text(ta_ssid);
    const char * pass = lv_textarea_get_text(ta_pass);
    
    // Update Text to User
    if (lbl_wifi_status) {
        lv_label_set_text(lbl_wifi_status, "Status: Connecting...");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFFFF00), 0); // Yellow
    }

    // Save to NVS
    save_wifi_creds(ssid, pass);
    
    // Update Runtime Config
    strncpy(wifi_ssid, ssid, sizeof(wifi_ssid));
    strncpy(wifi_pass, pass, sizeof(wifi_pass));
    
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_pass, sizeof(wifi_config.sta.password));
    
    // Reconnect
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();
}

static void wifi_ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if(kb) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER); // Default to text
        }
    } else if(code == LV_EVENT_DEFOCUSED) {
        if(kb) {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void create_settings_screen(void) {
    // Force Load from NVS to discard any unsaved RAM changes
    load_gauge_configs();
    
    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_layout(settings_screen, 0); // Disable flex to allow custom placement if needed, or keeping it clean
    // Actually, for TabView to take full space, better no flex on main container or simple layout.
    // Let's use basic container.

    // 1. TabView
    lv_obj_t * tabview = lv_tabview_create(settings_screen);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 50); // Correct API
    
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(90)); // Leave 10% bottom for Back button?
    lv_obj_align(tabview, LV_ALIGN_TOP_MID, 0, 0);
    
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x000000), 0);
    
    // Style Tab Buttons
    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x202020), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xFFFFFF), LV_STATE_CHECKED);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_20, 0);

    // --- Tab 1: Gauge Config ---
    lv_obj_t * tab1 = lv_tabview_add_tab(tabview, "Gauge Config");
    lv_obj_set_flex_flow(tab1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tab1, 20, 0);

    // Save Button
    lv_obj_t * btn_save = lv_btn_create(tab1);
    lv_obj_set_size(btn_save, 200, 50);
    lv_obj_add_event_cb(btn_save, save_config_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x00AA00), 0); // Green
    
    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SAVE CONFIG");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_save);
    
    // Dropdown to Select Gauge

    // Dropdown to Select Gauge
    lv_obj_t * dd = lv_dropdown_create(tab1);
    lv_dropdown_set_options(dd, "Gauge 1\nGauge 2\nGauge 3\nGauge 4\nGauge 5\nGauge 6\nGauge 7\nGauge 8\n"
                                "Gauge 9\nGauge 10\nGauge 11\nGauge 12\nGauge 13\nGauge 14\nGauge 15\nGauge 16");
    lv_obj_set_width(dd, 200);
    lv_dropdown_set_selected(dd, current_edit_index);
    lv_obj_add_event_cb(dd, dd_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_margin_bottom(dd, 10, 0);

    // Rows
    ta_name = create_config_row(tab1, "Gauge Name", gauge_configs[current_edit_index].name, 0, 1);
    ta_unit = create_config_row(tab1, "Gauge Unit", gauge_configs[current_edit_index].unit, 0, 2);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].min_val);
    ta_min = create_config_row(tab1, "Gauge Min", buf, 10, 0);
    
    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].max_val);
    ta_max = create_config_row(tab1, "Gauge Max", buf, 11, 0);
    
    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].blue_limit);
    ta_blue = create_config_row(tab1, "Blue Limit", buf, 12, 0);
    
    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].yellow_limit);
    ta_yellow = create_config_row(tab1, "Yellow Limit", buf, 13, 0);

    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].analog_min);
    ta_analog_min = create_config_row(tab1, "Analog In Min", buf, 14, 0);

    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].analog_max);
    ta_analog_max = create_config_row(tab1, "Analog In Max", buf, 15, 0);

    snprintf(buf, sizeof(buf), "%d", gauge_configs[current_edit_index].threshold);
    ta_threshold = create_config_row(tab1, "Threshold", buf, 16, 0);

    // Trigger Relay Dropdown
    lv_obj_t * lbl_trig = lv_label_create(tab1);
    lv_label_set_text(lbl_trig, "Trigger Relay (Independent):");
    lv_obj_set_style_text_color(lbl_trig, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_margin_top(lbl_trig, 10, 0);
    
    dd_trigger = lv_dropdown_create(tab1);
    lv_dropdown_set_options(dd_trigger, "None\nRelay 1\nRelay 2\nRelay 3\nRelay 4\nRelay 5\nRelay 6\nRelay 7\nRelay 8\n"
                                        "Relay 9\nRelay 10\nRelay 11\nRelay 12\nRelay 13\nRelay 14\nRelay 15\nRelay 16");
    lv_dropdown_set_selected(dd_trigger, gauge_configs[current_edit_index].trigger_relay_index);
    lv_obj_add_event_cb(dd_trigger, trigger_dd_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_width(dd_trigger, 200);

    // --- Tab 2: System Info ---
    lv_obj_t * tab2 = lv_tabview_add_tab(tabview, "System Info");
    lv_obj_set_flex_flow(tab2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Version
    lv_obj_t * lbl_ver = lv_label_create(tab2);
    // Use macro for version
    lv_label_set_text_fmt(lbl_ver, "App Version: v%s", "2.0.3"); 
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_margin_bottom(lbl_ver, 20, 0);
    
    // WiFi Status
    sys_wifi_label = lv_label_create(tab2);
    lv_label_set_text_fmt(sys_wifi_label, "WiFi Status: %s", wifi_connected ? "Connected" : "Disconnected"); 
    if(wifi_connected) lv_obj_set_style_text_color(sys_wifi_label, lv_color_hex(0x00FF00), 0);
    else lv_obj_set_style_text_color(sys_wifi_label, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_font(sys_wifi_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(sys_wifi_label, 20, 0);
    
    // MQTT Status
    sys_mqtt_label = lv_label_create(tab2);
    lv_label_set_text(sys_mqtt_label, "MQTT Status: Init...");
    lv_obj_set_style_text_font(sys_mqtt_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(sys_mqtt_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_margin_bottom(sys_mqtt_label, 20, 0);

    // IP Address
    sys_ip_label = lv_label_create(tab2);
    lv_label_set_text_fmt(sys_ip_label, "IP Address: %s", system_ip_str);
    lv_obj_set_style_text_font(sys_ip_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(sys_ip_label, lv_color_hex(0xAAAAAA), 0);


    // --- Tab 3: Safety Settings ---
    lv_obj_t * tab3 = lv_tabview_add_tab(tabview, "Safety");
    lv_obj_set_flex_flow(tab3, LV_FLEX_FLOW_COLUMN);
    
    // Siren Relay
    // ... (rest of Tab 3) ...

    // --- Tab 4: Activation ---\n    lv_obj_t * tab4 = lv_tabview_add_tab(tabview, "Activation");\n    lv_obj_set_flex_flow(tab4, LV_FLEX_FLOW_ROW_WRAP);\n    lv_obj_set_flex_align(tab4, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);\n    lv_obj_set_style_pad_all(tab4, 10, 0);\n    lv_obj_set_style_pad_gap(tab4, 20, 0);\n\n    for(int i=0; i<16; i++) {\n        lv_obj_t * cb = lv_checkbox_create(tab4);\n        lv_checkbox_set_text_fmt(cb, "Gauge %d", i+1);\n        lv_obj_set_style_text_font(cb, &lv_font_montserrat_20, 0);\n        lv_obj_set_style_text_color(cb, lv_color_hex(0xFFFFFF), 0);\n        \n        // Set Initial State\n        if(gauge_active_mask & (1 << i)) {\n            lv_obj_add_state(cb, LV_STATE_CHECKED);\n        }\n        \n        lv_obj_add_event_cb(cb, activation_checkbox_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);\n    }\nbj_set_style_text_color(lbl_siren, lv_color_hex(0xFFFFFF), 0);
    
    lv_obj_t * dd_siren = lv_dropdown_create(tab3);
    lv_dropdown_set_options(dd_siren, "None\nRelay 1\nRelay 2\nRelay 3\nRelay 4\nRelay 5\nRelay 6\nRelay 7\nRelay 8\n"
                                        "Relay 9\nRelay 10\nRelay 11\nRelay 12\nRelay 13\nRelay 14\nRelay 15\nRelay 16");
    lv_dropdown_set_selected(dd_siren, safety_config.siren_relay_index);
    lv_obj_add_event_cb(dd_siren, siren_dd_cb, LV_EVENT_ALL, NULL);
    
    // Siren Invert
    lv_obj_t * cont_sw_siren = lv_obj_create(tab3);
    lv_obj_set_size(cont_sw_siren, 300, 50);
    lv_obj_set_style_bg_opa(cont_sw_siren, 0, 0);
    lv_obj_set_style_border_width(cont_sw_siren, 0, 0);
    lv_obj_set_flex_flow(cont_sw_siren, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_sw_siren, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t * lbl_sw_siren = lv_label_create(cont_sw_siren);
    lv_label_set_text(lbl_sw_siren, "Invert Siren Output");
    lv_obj_set_style_text_color(lbl_sw_siren, lv_color_hex(0xFFFFFF), 0);
    
    lv_obj_t * sw_siren = lv_switch_create(cont_sw_siren);
    if(safety_config.siren_invert) lv_obj_add_state(sw_siren, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_siren, siren_sw_cb, LV_EVENT_ALL, NULL);

    // Strobe Relay
    lv_obj_t * lbl_strobe = lv_label_create(tab3);
    lv_label_set_text(lbl_strobe, "Strobe Relay:");
    lv_obj_set_style_text_color(lbl_strobe, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_margin_top(lbl_strobe, 20, 0);
    
    lv_obj_t * dd_strobe = lv_dropdown_create(tab3);
    lv_dropdown_set_options(dd_strobe, "None\nRelay 1\nRelay 2\nRelay 3\nRelay 4\nRelay 5\nRelay 6\nRelay 7\nRelay 8\n"
                                         "Relay 9\nRelay 10\nRelay 11\nRelay 12\nRelay 13\nRelay 14\nRelay 15\nRelay 16");
    lv_dropdown_set_selected(dd_strobe, safety_config.strobe_relay_index);
    lv_obj_add_event_cb(dd_strobe, strobe_dd_cb, LV_EVENT_ALL, NULL);

    // Strobe Invert
    lv_obj_t * cont_sw_strobe = lv_obj_create(tab3);
    lv_obj_set_size(cont_sw_strobe, 300, 50);
    lv_obj_set_style_bg_opa(cont_sw_strobe, 0, 0);
    lv_obj_set_style_border_width(cont_sw_strobe, 0, 0);
    lv_obj_set_flex_flow(cont_sw_strobe, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_sw_strobe, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t * lbl_sw_strobe = lv_label_create(cont_sw_strobe);
    lv_label_set_text(lbl_sw_strobe, "Invert Strobe Output");
    lv_obj_set_style_text_color(lbl_sw_strobe, lv_color_hex(0xFFFFFF), 0);
    
    lv_obj_t * sw_strobe = lv_switch_create(cont_sw_strobe);
    if(safety_config.strobe_invert) lv_obj_add_state(sw_strobe, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_strobe, strobe_sw_cb, LV_EVENT_ALL, NULL);

    // --- Tab 4: Activation ---
    lv_obj_t * tab4 = lv_tabview_add_tab(tabview, "Activation");
    lv_obj_set_flex_flow(tab4, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(tab4, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(tab4, 10, 0);
    lv_obj_set_style_pad_gap(tab4, 20, 0);

    for(int i=0; i<16; i++) {
        lv_obj_t * cb = lv_checkbox_create(tab4);
        char cb_txt[20];
        snprintf(cb_txt, sizeof(cb_txt), "Gauge %d", i+1);
        lv_checkbox_set_text(cb, cb_txt);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(cb, lv_color_hex(0xFFFFFF), 0);
        
        // Set Initial State
        if(gauge_active_mask & (1 << i)) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
        }
        
        lv_obj_add_event_cb(cb, activation_checkbox_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
    }

    // --- Tab 5: WiFi Setup ---
    lv_obj_t * tab5 = lv_tabview_add_tab(tabview, "WiFi Setup");
    lv_obj_set_flex_flow(tab5, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab5, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tab5, 30, 0);

    // SSID Input
    lv_obj_t * lbl_ssid = lv_label_create(tab5);
    lv_label_set_text(lbl_ssid, "WiFi SSID:");
    lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(0xFFFFFF), 0);
    
    ta_ssid = lv_textarea_create(tab5);
    lv_textarea_set_one_line(ta_ssid, true);
    lv_textarea_set_text(ta_ssid, wifi_ssid);
    lv_obj_set_width(ta_ssid, 300);
    lv_obj_add_event_cb(ta_ssid, wifi_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_margin_bottom(ta_ssid, 20, 0);

    // Password Input
    lv_obj_t * lbl_pass = lv_label_create(tab5);
    lv_label_set_text(lbl_pass, "Password:");
    lv_obj_set_style_text_color(lbl_pass, lv_color_hex(0xFFFFFF), 0);
    
    ta_pass = lv_textarea_create(tab5);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_password_mode(ta_pass, true); // Hidden
    lv_textarea_set_text(ta_pass, wifi_pass);
    lv_obj_set_width(ta_pass, 300);
    lv_obj_add_event_cb(ta_pass, wifi_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_margin_bottom(ta_pass, 20, 0);

    // Connect Button
    lv_obj_t * btn_connect = lv_btn_create(tab5);
    lv_obj_set_size(btn_connect, 150, 50);
    lv_obj_add_event_cb(btn_connect, wifi_connect_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_connect, lv_color_hex(0x00AA00), 0); // Green
    
    lv_obj_t * lbl_btn = lv_label_create(btn_connect);
    lv_label_set_text(lbl_btn, "Connect");
    lv_obj_center(lbl_btn);
    
    // Status Label
    lbl_wifi_status = lv_label_create(tab5);
    lv_label_set_text(lbl_wifi_status, "Status: Idle");
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_margin_top(lbl_wifi_status, 20, 0);

    // 2. Back Button (Bottom Fixed)
    lv_obj_t * btn = lv_btn_create(settings_screen);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn, back_from_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x505050), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_UP " Back");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
    
    // Keyboard (Global on top)
    kb = lv_keyboard_create(settings_screen);
    lv_obj_set_size(kb, 800, 300); // Half Screen
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0); // Stick to bottom
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // Hide initially
}

// Helper to get widget objects by index (store pointers if needed, or iterate children)
// For simplicity, we'll traverse children of main_screen or assume order.
// Better approach: Store widget pointers in an array.
// Global arrays defined at top

static lv_obj_t* create_gas_widget(lv_obj_t *parent, int index) {
    // 1. Container (Black Card, Rounded)
    // Resized for Compact Fit (Target 4 cols in ~1100px width) -> ~260px wide
    lv_obj_t * container = lv_obj_create(parent);
    lv_obj_set_size(container, 260, 330); 
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0); // Black
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(container, 20, 0); 
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 5, 0); // Minimal padding
    // Center in cell handled by grid/flex

    // 2. Arc Gauge (270 degree)
    lv_obj_t * arc = lv_arc_create(container);
    lv_obj_set_size(arc, 200, 200); // Smaller arc
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, gauge_configs[index].min_val, gauge_configs[index].max_val);
    lv_arc_set_value(arc, 0);
    // Align Arc slightly higher to reduce top gap
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 30); 
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    
    // Style: Dark Grey Background Arc
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 15, LV_PART_MAIN); // Thinner
    
    // Style: Indicator Arc
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR); // Cyan default
    lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    
    gauge_arcs[index] = arc;

    // 3. Digital Value
    lv_obj_t * val_lbl = lv_label_create(container);
    lv_label_set_text(val_lbl, "0");
    lv_obj_set_width(val_lbl, 200); // Fixed Width (Matches Arc)
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_CLIP); // Prevent auto-resize width
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_CENTER, 0); // Force Center Text
    // Align inside arc center
    lv_obj_align_to(val_lbl, arc, LV_ALIGN_CENTER, 0, 0); 
    
    gauge_labels[index] = val_lbl;

    // 4. Label NAME (Above Arc, closer to top)
    lv_obj_t * title_lbl = lv_label_create(container);
    lv_label_set_text(title_lbl, gauge_configs[index].name); 
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 10); // Very top
    
    gauge_title_labels[index] = title_lbl;

    // 5. Label "PPM" (Below Value, inside Arc)
    lv_obj_t * unit_lbl = lv_label_create(container);
    lv_label_set_text(unit_lbl, gauge_configs[index].unit); 
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_12, 0); 
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    gauge_unit_labels[index] = unit_lbl;
    
    // 6. Status Label (SAFE / WARNING) - Above Button
    lv_obj_t * status_lbl = lv_label_create(container);
    lv_label_set_text(status_lbl, "SAFE");
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_20, 0); // Larger Font
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFFFFFF), 0); // White Text
    lv_obj_set_style_bg_color(status_lbl, lv_color_hex(0x00FF00), 0); // Green Background
    lv_obj_set_style_bg_opa(status_lbl, LV_OPA_COVER, 0); // Solid
    lv_obj_set_style_radius(status_lbl, 6, 0); // Slightly more rounded
    lv_obj_set_style_pad_all(status_lbl, 8, 0); // Larger Padding
    lv_obj_align(status_lbl, LV_ALIGN_BOTTOM_MID, 0, -70); // Adjusted Position
    
    gauge_status_labels[index] = status_lbl;

    // 7. Trending Button (Bottom)
    lv_obj_t * btn = lv_btn_create(container);
    lv_obj_set_size(btn, 100, 30);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x505050), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_radius(btn, 30, 0);
    lv_obj_add_event_cb(btn, trending_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);

    lv_obj_t * btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "TRENDING");
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0); // Smaller
    lv_obj_center(btn_lbl);

    gauge_widgets[index] = container; // Store reference
    return container;
}


// Update all 16 gauges - Simulation Mode
static void gas_update_timer_cb(lv_timer_t * timer) {
    // Only update gauges for the CURRENT page to save performance
    // Page 0: 0-7, Page 1: 8-15
    if (current_page < 2) {
        int start_idx = current_page * 8;
        int end_idx = start_idx + 8;

        for (int i = start_idx; i < end_idx; i++) {
            if (gauge_arcs[i] && gauge_labels[i]) {
                
                // Read from Modbus Data
                int raw_val = sys_modbus_data.analog_vals[i];
                
                // Map Value
                long in_min = gauge_configs[i].analog_min;
                long in_max = gauge_configs[i].analog_max;
                long out_min = gauge_configs[i].min_val;
                long out_max = gauge_configs[i].max_val;
                
                int val = raw_val; // Default
                if ((in_max - in_min) != 0) {
                     val = (int)((raw_val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
                }

                // Final Safety Clamp (Moved here to affect UI)
                if (val < 0) val = 0;

                // --- Activation Logic (UI) ---
                if (!(gauge_active_mask & (1 << i))) {
                    // Gauge Inactive
                    if(gauge_arcs[i]) {
                         lv_arc_set_value(gauge_arcs[i], 0);
                         lv_obj_set_style_opa(gauge_arcs[i], LV_OPA_30, 0);
                         lv_obj_t * parent = lv_obj_get_parent(gauge_arcs[i]);
                         if(parent) lv_obj_set_style_opa(parent, LV_OPA_30, 0);
                    }
                    if(gauge_labels[i]) {
                        lv_label_set_text(gauge_labels[i], "0");
                        lv_obj_set_style_opa(gauge_labels[i], LV_OPA_30, 0);
                    }
                    continue; // Skip visual update
                } else {
                     // Gauge Active - Restore Opacity
                    if(gauge_arcs[i]) {
                         lv_obj_set_style_opa(gauge_arcs[i], LV_OPA_COVER, 0);
                         lv_obj_t * parent = lv_obj_get_parent(gauge_arcs[i]);
                         if(parent) lv_obj_set_style_opa(parent, LV_OPA_COVER, 0);
                    }
                    if(gauge_labels[i]) {
                        lv_obj_set_style_opa(gauge_labels[i], LV_OPA_COVER, 0);
                    }
                }

                // Check connection status
                bool connected = (i < 8) ? sys_modbus_data.connected[0] : sys_modbus_data.connected[1];
                
                if (!connected) {
                     lv_obj_set_style_arc_color(gauge_arcs[i], lv_color_hex(0x505050), LV_PART_INDICATOR); // Grey if disconnected
                }

                // Update Arc (Only if changed)
                if (lv_arc_get_value(gauge_arcs[i]) != val) {
                    lv_arc_set_value(gauge_arcs[i], val);
                    lv_label_set_text_fmt(gauge_labels[i], "%d", val);
                }
                
                // Dynamic Color Logic (Optimized: Only update on state change)
                static int last_zone[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
                int current_zone = 0; // 0: Blue, 1: Yellow, 2: Red

                if (val <= gauge_configs[i].blue_limit) current_zone = 0;
                else if (val <= gauge_configs[i].yellow_limit) current_zone = 1;
                else current_zone = 2;

                if (last_zone[i] != current_zone) {
                    if (current_zone == 0) lv_obj_set_style_arc_color(gauge_arcs[i], lv_color_hex(0x00FFFF), LV_PART_INDICATOR);
                    else if (current_zone == 1) lv_obj_set_style_arc_color(gauge_arcs[i], lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
                    else lv_obj_set_style_arc_color(gauge_arcs[i], lv_color_hex(0xFF0000), LV_PART_INDICATOR);
                    
                    last_zone[i] = current_zone;
                }
                
                // Status Label Logic
                if (gauge_status_labels[i]) {
                     if (val > gauge_configs[i].threshold) {
                         // Threshold Exceeded -> Warning
                         lv_label_set_text(gauge_status_labels[i], "WARNING");
                         lv_obj_set_style_bg_color(gauge_status_labels[i], lv_color_hex(0xFF0000), 0); // Red Box
                     } else {
                         // Below Threshold -> Safe
                         lv_label_set_text(gauge_status_labels[i], "SAFE");
                         lv_obj_set_style_bg_color(gauge_status_labels[i], lv_color_hex(0x00FF00), 0); // Green Box
                     }
                }
            }
        }
    }
    
    // --- Safety / Master Warning Logic ---
    static int last_siren_state = -1; 
    static int last_strobe_state = -1;
    bool any_alarm_active = false;
    
    // We check all 16 gauges
    for(int i=0; i<16; i++) {
        // Skip Inactive Gauges for Alarm
        if (!(gauge_active_mask & (1 << i))) continue;

        bool connected = (i < 8) ? sys_modbus_data.connected[0] : sys_modbus_data.connected[1];
        if(!connected) continue; 
        
        int raw_val = sys_modbus_data.analog_vals[i];
        long in_min = gauge_configs[i].analog_min;
        long in_max = gauge_configs[i].analog_max;
        long out_min = gauge_configs[i].min_val;
        long out_max = gauge_configs[i].max_val;
        
        int val = raw_val; 
        
        // Restore Scaling Logic for Loop 2
        if ((in_max - in_min) != 0) {
             val = (int)((raw_val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
        }
        
        // Final Safety Clamp
        if (val < 0) val = 0;

        // Update UI only for visible gauges
        if (gauge_arcs[i] && gauge_labels[i]) {
            // Check connection status
            // This logic is now redundant here as we check `connected` at the top of the loop
            // and `gauge_arcs[i]` is only updated if connected.
            // if (!connected) {
            //      lv_obj_set_style_arc_color(gauge_arcs[i], lv_color_hex(0x505050), LV_PART_INDICATOR); // Grey if disconnected
            // }

            // Update Arc (Duplicate UI update? No, just logic check on visual objects if they exist)
            // Actually, the previous loop handles UI. This loop seems to duplicate UI updates IF visual objects exist.
            // I will leave it be to minimalize diff risk, but the `gauge_active_mask` check at top handles safety.
            // Oh wait, lines 1378-1398 in original (inside this second loop) DO update UI again.
            // This is inefficient code structure (duplicated logic).
            // But since I added mask check at top of THIS loop, it triggers `continue` so it won't update UI or trigger alarm.
            // EXCEPT: If I `continue` early, I don't force it to 0/fade here.
            // BUT: I forced it to 0/fade in the FIRST loop.
            // So as long as BOTH loops run, it's fine.
            
            // --- Safety Logic ---
            // Trigger if Value > Threshold
            // EXTREME DEBUGGING
            if(i==0) { 
                static int d_div = 0;
                if(d_div++ > 20) {
                    ESP_LOGW(TAG, "G[0] Val: %d | Thresh: %d | Active? %s", 
                        val, gauge_configs[i].threshold, (val > gauge_configs[i].threshold) ? "YES" : "NO");
                    d_div = 0;
                }
            }
            
            if (val > gauge_configs[i].threshold) {
                any_alarm_active = true;
                
                // Add to chart if Live Mode is active and this is the selected gauge
                if (trending_live_mode && 
                    trending_screen && 
                    lv_scr_act() == trending_screen && 
                    current_trending_index == i && 
                    const_chart && 
                    const_ser1) {
                    
                    lv_chart_set_next_value(const_chart, const_ser1, val);
                }
                // Update Warning Screen Source Text (Show the FIRST one found for now, or cycle? First is fine)
                if (lbl_warning_source) {
                    lv_label_set_text_fmt(lbl_warning_source, "Source: %s\nLevel: %d %s", 
                                          gauge_configs[i].name, val, gauge_configs[i].unit);
                }
            }
        }
    }
    
    // UI Updates based on Alarm
    if(any_alarm_active) {
        // Show Warning Label
        if(warning_label) lv_obj_clear_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
        
        // Create Warning Screen Logic
        if(!alarm_acknowledged) {
            // Hardware Acknowledge Check (Button 2, Index 1)
            // Assumes sys_modbus_data is active and polling
            // UPDATED: User requested Button 2 for Ack
            if (sys_modbus_data.buttons[1]) {
                 perform_acknowledge();
                 ESP_LOGW(TAG, "Hardware Ack Detected (Btn 2)!");
            }

            // Check if we are already on warning screen
            lv_obj_t * act_scr = lv_scr_act();
            if (act_scr != warning_screen) {
                last_active_screen = act_scr; // Save current screen (Main or Settings)
                ESP_LOGE(TAG, "ALARM TRIGGERED! Switching to Warning Screen.");
                lv_scr_load_anim(warning_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
            }
        }
    } else {
        // Clear everything if safe
        alarm_acknowledged = false; // Reset ack
        if(warning_label) lv_obj_add_flag(warning_label, LV_OBJ_FLAG_HIDDEN);
        
        // If we clear while on warning screen, go back?
        // User didn't specify, but makes sense.
        if (lv_scr_act() == warning_screen) {
             if (last_active_screen) lv_scr_load(last_active_screen);
             else lv_scr_load(main_screen);
        }
    }

    // Siren Logic
    // --- Relay Logic Implementation ---
    // We calculate the desired state for ALL relays managed by alarms (Siren, Strobe, Gauge Triggers).
    // We then update them diff-intelligently.

    bool relay_targets[16] = {false};
    bool relay_managed[16] = {false}; // Track which relays are controlled by this logic

    // 1. Independent Gauge Triggers
    for(int i=0; i<16; i++) {
        // Only if gauge active and alarm active (red_active logic from above loop?)
        // I need to know if THIS gauge is in alarm.
        // The check was loop-local above. I should record it or re-check.
        // Re-checking is cheap.
        if (gauge_active_mask & (1 << i)) {
             long val_check = sys_modbus_data.analog_vals[i];
             // Scale if needed or check raw? using raw for now as per previous logic assumptions or consistency.
             int t_idx = gauge_configs[i].trigger_relay_index;
             if (t_idx > 0 && t_idx <= 16) {
                 relay_managed[t_idx-1] = true;
                 
                 // Re-evaluate alarm condition for this gauge
                 bool is_alarm = false;
                 // Assuming Red Limit Logic
                 long out_min = gauge_configs[i].min_val;
                 long out_max = gauge_configs[i].max_val;
                 // Need normalized value?
                 // The red_limit is in user units.
                 // Let's use the loop above to store alarm state?
                 // Or easier: Just re-calculate normalized value
                 float norm_val = (float)sys_modbus_data.analog_vals[i];
                  long in_min = gauge_configs[i].analog_min;
                  long in_max = gauge_configs[i].analog_max;
                  if ((in_max - in_min) != 0) {
                       norm_val = (float)((sys_modbus_data.analog_vals[i] - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
                  }
                  
                  
                  if (norm_val >= gauge_configs[i].threshold) { 
                      relay_targets[t_idx-1] = true;
                  }
             }
        }
    }

    // 2. Siren Logic
    bool target_siren = any_alarm_active && !alarm_acknowledged;
    if (safety_config.siren_invert) target_siren = !target_siren;
    
    if (safety_config.siren_relay_index > 0) {
        int r_idx = safety_config.siren_relay_index - 1;
        relay_managed[r_idx] = true;
        relay_targets[r_idx] = target_siren; // Overwrite gauge trigger if conflict (Priority: Siren)
    }

    // 3. Strobe Logic
    bool target_strobe = any_alarm_active; // Ack doesn't stop strobe
    if (safety_config.strobe_invert) target_strobe = !target_strobe;

    if (safety_config.strobe_relay_index > 0) {
        int r_idx = safety_config.strobe_relay_index - 1;
        relay_managed[r_idx] = true;
        relay_targets[r_idx] = target_strobe; // Overwrite gauge trigger if conflict (Priority: Strobe)
    }

    // 4. Apply Changes
    static bool last_managed_state[16] = {false};
    static bool initialized_relays = false;
    
    // Invert output logic not applied here? 
    // Wait, Siren/Strobe have specialized invert flags. 
    // Gauge Triggers do NOT have invert flags in the UI. Assuming Active HIGH (ON).
    
    for(int i=0; i<16; i++) {
        if (relay_managed[i]) {
            // Check if changed or force update periodically?
            // Let's check against sys_modbus_data.relays (current status) to be robust against manual toggles?
            // Or strictly enforce our target?
            // "Calculated target" vs "Last written target".
            // Let's strictly enforce if it differs from our INTENDED state.
            
            if (!initialized_relays || last_managed_state[i] != relay_targets[i]) {
                 esp_err_t err = modbus_set_relay(i, relay_targets[i]);
                 if(err == ESP_OK) {
                     last_managed_state[i] = relay_targets[i];
                     ESP_LOGD(TAG, "Managed Relay %d set to %d", i+1, relay_targets[i]);
                 }
            }
        }
    }
    initialized_relays = true;
    
    // Update Chart (Specific to Trending Page)
    if (const_chart && trending_screen && lv_scr_act() == trending_screen) {
         int idx = current_trending_index;
         int raw_val = sys_modbus_data.analog_vals[idx];
         long in_min = gauge_configs[idx].analog_min;
         long in_max = gauge_configs[idx].analog_max;
         long out_min = gauge_configs[idx].min_val;
         long out_max = gauge_configs[idx].max_val;
         
         int val = raw_val; 
         if ((in_max - in_min) != 0) {
              val = (int)((raw_val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
         }
         
         lv_chart_set_next_value(const_chart, const_ser1, val);
         lv_chart_refresh(const_chart); 
    }
    
    // Update Modbus Status Label
    if (mb_status_label && lv_obj_is_valid(mb_status_label)) {
        bool any_connected = false;
        bool all_connected = true;
        for(int k=0; k<4; k++) {
            if(sys_modbus_data.connected[k]) any_connected = true;
            else all_connected = false;
        }

        static int last_mb_status = -1; 
        int current_mb_status = 2;

        if (all_connected) current_mb_status = 0;
        else if (any_connected) current_mb_status = 1;
        else current_mb_status = 2;

        if (last_mb_status != current_mb_status) {
            if (current_mb_status == 0) {
                lv_label_set_text(mb_status_label, "MB: OK");
                lv_obj_set_style_text_color(mb_status_label, lv_color_hex(0x00FF00), 0);
            } else if (current_mb_status == 1) {
                 lv_label_set_text(mb_status_label, "MB: PARTIAL");
                 lv_obj_set_style_text_color(mb_status_label, lv_color_hex(0xFFFF00), 0);
            } else {
                 lv_label_set_text(mb_status_label, "MB: ERR");
                 lv_obj_set_style_text_color(mb_status_label, lv_color_hex(0xFF0000), 0);
            }
            lv_obj_move_foreground(mb_status_label); 
            last_mb_status = current_mb_status;
        }
    }

    // Update Page 3 (Relays & Buttons)
    if (current_page == 2) {
        if (grid_page_3 != NULL && lv_obj_is_valid(grid_page_3)) {
             // Relays
            for(int i=0; i<16; i++) {
                if (relay_leds[i] && lv_obj_is_valid(relay_leds[i])) { 
                    if(sys_modbus_data.relays[i]) lv_led_on(relay_leds[i]);
                    else lv_led_off(relay_leds[i]);
                    
                    // Dim if disconnected
                    bool connected = (i < 8) ? sys_modbus_data.connected[2] : sys_modbus_data.connected[3];
                    if(!connected) lv_led_set_color(relay_leds[i], lv_color_hex(0x505050));
                    else lv_led_set_color(relay_leds[i], lv_color_hex(0x00FF00));
                }
            }
            // Buttons
             for(int i=0; i<4; i++) {
                if (input_leds[i] && lv_obj_is_valid(input_leds[i])) { 
                    if(sys_modbus_data.buttons[i]) lv_led_on(input_leds[i]);
                    else lv_led_off(input_leds[i]);
                    
                    // Using ID4 Status
                     if(!sys_modbus_data.connected[3]) lv_led_set_color(input_leds[i], lv_color_hex(0x505050));
                     else lv_led_set_color(input_leds[i], lv_color_hex(0x00FF00));
                }
            }
        }
    }
}



static void next_page_cb(lv_event_t * e) {
    if (current_page == 0) {
        current_page = 1;
        lv_obj_add_flag(grid_page_1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(grid_page_2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_prev, LV_OBJ_FLAG_HIDDEN); // Show Prev
    } else if (current_page == 1) {
        if (grid_page_3 != NULL) {
            current_page = 2; // Page 3
            lv_obj_add_flag(grid_page_2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(grid_page_3, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_next, LV_OBJ_FLAG_HIDDEN); // Hide Next on Last Page
        }
    }
}

static void prev_page_cb(lv_event_t * e) {
    if (current_page == 2) {
        if (grid_page_2 != NULL) {
             current_page = 1;
             lv_obj_add_flag(grid_page_3, LV_OBJ_FLAG_HIDDEN);
             lv_obj_clear_flag(grid_page_2, LV_OBJ_FLAG_HIDDEN);
             lv_obj_clear_flag(btn_next, LV_OBJ_FLAG_HIDDEN); // Show Next
        }
    } else if (current_page == 1) {
        current_page = 0;
        lv_obj_add_flag(grid_page_2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(grid_page_1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_prev, LV_OBJ_FLAG_HIDDEN); // Hide Prev on First Page
    }
}

// --- Service Screen & Logic ---

static lv_obj_t * pin_screen = NULL;
static lv_obj_t * pin_ta = NULL;
static lv_obj_t * history_table = NULL;
static void create_pin_screen(void); // Forward declaration

static lv_obj_t * dd_day;
static lv_obj_t * dd_month;
static lv_obj_t * dd_year;
static lv_obj_t * expiry_dd;
// static lv_obj_t * service_kb; // Removed

// --- PIN Logic ---
static void pin_kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY) { // Enter Key pressed
        const char * txt = lv_textarea_get_text(pin_ta);
        if (strcmp(txt, "8888") == 0) {
            // Success
            lv_textarea_set_text(pin_ta, "");
            create_service_screen();
            lv_scr_load(service_screen);
        } else {
            // Fail
            lv_textarea_set_text(pin_ta, "");
        }
    } else if (code == LV_EVENT_CANCEL) {
        lv_textarea_set_text(pin_ta, "");
        lv_scr_load(main_screen);
    }
}

static void create_pin_screen(void) {
    if (pin_screen) lv_obj_del(pin_screen);
    pin_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pin_screen, lv_color_hex(0x000000), 0);
    
    lv_obj_t * title = lv_label_create(pin_screen);
    lv_label_set_text(title, "ENTER TECHNICIAN PIN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
    
    pin_ta = lv_textarea_create(pin_screen);
    lv_textarea_set_password_mode(pin_ta, true);
    lv_textarea_set_one_line(pin_ta, true);
    lv_textarea_set_max_length(pin_ta, 8);
    lv_obj_set_width(pin_ta, 200);
    lv_obj_align(pin_ta, LV_ALIGN_TOP_MID, 0, 90);
    
    lv_obj_t * kb = lv_keyboard_create(pin_screen);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, pin_ta);
    lv_obj_add_event_cb(kb, pin_kb_event_cb, LV_EVENT_ALL, NULL);
}

static void preview_back_cb(lv_event_t * e) {
    if (service_screen) lv_scr_load(service_screen);
    else lv_scr_load(main_screen);
}

static void service_preview_cb(lv_event_t * e) {
    create_reminder_screen(true); // Preview Mode
    lv_scr_load(reminder_screen);
}

// --- Updated Service Screen ---

// --- Updated Service Screen ---
static void service_back_cb(lv_event_t * e) {
    // Save Values from Dropdowns
    // IDs are 0-indexed
    int d = lv_dropdown_get_selected(dd_day) + 1;     // 0 -> 1, 30 -> 31
    int m = lv_dropdown_get_selected(dd_month) + 1;   // 0 -> 1, 11 -> 12
    int y = lv_dropdown_get_selected(dd_year) + 2025; // 0 -> 2025
    
    // Basic Validation (Implicitly accurate range via dropdowns)
    if (d > 0 && d <= 31 && m > 0 && m <= 12 && y >= 2025 && y <= 2035) {
        // Update History: Shift down
        for(int i=5; i>0; i--) {
            service_config.history_year[i] = service_config.history_year[i-1];
            service_config.history_month[i] = service_config.history_month[i-1];
            service_config.history_day[i] = service_config.history_day[i-1];
        }
        // Insert new at top
        service_config.history_year[0] = y;
        service_config.history_month[0] = m;
        service_config.history_day[0] = d;

        service_config.cal_year = y;
        service_config.cal_month = m;
        service_config.cal_day = d;
    }
    
    service_config.expiry_period = lv_dropdown_get_selected(expiry_dd);
    
    save_gauge_configs(); // Save to NVS
    
    lv_scr_load(main_screen);
}

static void create_service_screen(void) {
    if (service_screen) lv_obj_del(service_screen);
    service_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(service_screen, lv_color_hex(0x101010), 0); // Dark Gray
    
    // Title
    lv_obj_t * title = lv_label_create(service_screen);
    lv_label_set_text(title, "CALIBRATION SETUP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00D0FF), 0); // Cyan
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // -- Left Side: Inputs --
    int input_x = 40;
    int input_y = 80;
    
    // Day
    lv_obj_t * lbl_d = lv_label_create(service_screen);
    lv_label_set_text(lbl_d, "Day");
    lv_obj_align(lbl_d, LV_ALIGN_TOP_LEFT, input_x, input_y);
    
    dd_day = lv_dropdown_create(service_screen);
    static char day_opts[128] = "";
    if(day_opts[0] == 0) {
        for(int i=1; i<=31; i++) {
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%d\n", i);
            strcat(day_opts, tmp);
        }
        day_opts[strlen(day_opts)-1] = 0;
    }
    lv_dropdown_set_options(dd_day, day_opts);
    lv_obj_set_width(dd_day, 80);
    lv_obj_align(dd_day, LV_ALIGN_TOP_LEFT, input_x, input_y + 30);
    if(service_config.cal_day >= 1 && service_config.cal_day <= 31)
        lv_dropdown_set_selected(dd_day, service_config.cal_day - 1);

    // Month
    lv_obj_t * lbl_m = lv_label_create(service_screen);
    lv_label_set_text(lbl_m, "Month");
    lv_obj_align(lbl_m, LV_ALIGN_TOP_LEFT, input_x + 100, input_y);
    
    dd_month = lv_dropdown_create(service_screen);
    lv_dropdown_set_options(dd_month, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12");
    lv_obj_set_width(dd_month, 80);
    lv_obj_align(dd_month, LV_ALIGN_TOP_LEFT, input_x + 100, input_y + 30);
    if(service_config.cal_month >= 1 && service_config.cal_month <= 12)
        lv_dropdown_set_selected(dd_month, service_config.cal_month - 1);
    
    // Year
    lv_obj_t * lbl_y = lv_label_create(service_screen);
    lv_label_set_text(lbl_y, "Year");
    lv_obj_align(lbl_y, LV_ALIGN_TOP_LEFT, input_x + 200, input_y);
    
    dd_year = lv_dropdown_create(service_screen);
    lv_dropdown_set_options(dd_year, "2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035");
    lv_obj_set_width(dd_year, 110);
    lv_obj_align(dd_year, LV_ALIGN_TOP_LEFT, input_x + 200, input_y + 30);
    if(service_config.cal_year >= 2025 && service_config.cal_year <= 2035)
        lv_dropdown_set_selected(dd_year, service_config.cal_year - 2025);

    // Expiry (Remind In)
    lv_obj_t * lbl_exp = lv_label_create(service_screen);
    lv_label_set_text(lbl_exp, "Remind In:");
    lv_obj_align(lbl_exp, LV_ALIGN_TOP_LEFT, input_x, input_y + 90); 
    
    expiry_dd = lv_dropdown_create(service_screen);
    lv_dropdown_set_options(expiry_dd, "6 Months\n1 Year");
    lv_dropdown_set_selected(expiry_dd, service_config.expiry_period);
    lv_obj_set_width(expiry_dd, 180);
    lv_obj_align(expiry_dd, LV_ALIGN_TOP_LEFT, input_x, input_y + 120);

    // -- Right Side: History --
    history_table = lv_table_create(service_screen);
    lv_obj_set_size(history_table, 320, 240);
    lv_obj_align(history_table, LV_ALIGN_TOP_RIGHT, -40, 80);
    lv_table_set_col_cnt(history_table, 2);
    lv_table_set_col_width(history_table, 0, 70); 
    lv_table_set_col_width(history_table, 1, 220); 
    
    lv_table_set_cell_value(history_table, 0, 0, "#");
    lv_table_set_cell_value(history_table, 0, 1, "Date");
    
    for(int i=0; i<6; i++) {
        char num_str[4];
        snprintf(num_str, sizeof(num_str), "%d", i+1);
        lv_table_set_cell_value(history_table, i+1, 0, num_str);
        
        if (service_config.history_year[i] > 2000) {
            char date_str[32];
            snprintf(date_str, sizeof(date_str), "%02d/%02d/%d", 
                service_config.history_day[i], service_config.history_month[i], service_config.history_year[i]);
            lv_table_set_cell_value(history_table, i+1, 1, date_str);
        } else {
            lv_table_set_cell_value(history_table, i+1, 1, "-");
        }
    }
    
    lv_obj_t * hist_title = lv_label_create(service_screen);
    lv_label_set_text(hist_title, "HISTORY");
    lv_obj_set_style_text_font(hist_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(hist_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align_to(hist_title, history_table, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // -- Buttons at Bottom Corners --
    
    // Preview Button (Bottom Left)
    lv_obj_t * btn_preview = lv_btn_create(service_screen);
    lv_obj_set_size(btn_preview, 180, 60);
    lv_obj_align(btn_preview, LV_ALIGN_BOTTOM_LEFT, 40, -30);
    lv_obj_add_event_cb(btn_preview, service_preview_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_preview = lv_label_create(btn_preview);
    lv_label_set_text(lbl_preview, "PREVIEW");
    lv_obj_center(lbl_preview);

    // Save Button (Bottom Right)
    lv_obj_t * btn_save = lv_btn_create(service_screen);
    lv_obj_set_size(btn_save, 180, 60);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -40, -30);
    lv_obj_add_event_cb(btn_save, service_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SAVE & EXIT");
    lv_obj_center(lbl_save);
}

static void reminder_ack_cb(lv_event_t * e) {
    // Acknowledge
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    service_config.last_ack_year = timeinfo.tm_year + 1900;
    service_config.last_ack_month = timeinfo.tm_mon + 1;
    service_config.last_ack_day = timeinfo.tm_mday;
    
    save_gauge_configs();
    
    if (last_active_screen) lv_scr_load(last_active_screen);
    else lv_scr_load(main_screen);
}

static void create_reminder_screen(bool is_preview) {
    if (reminder_screen) lv_obj_del(reminder_screen);
    reminder_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(reminder_screen, lv_color_hex(0x200000), 0); // Dark Red Tint
    
    lv_obj_t * title = lv_label_create(reminder_screen);
    lv_label_set_text(title, "SERVICE REMINDER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0); // Bold/Large
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF0000), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    lv_obj_t * msg = lv_label_create(reminder_screen);
    lv_label_set_text(msg, "Please contact:");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 100);

    // Logo
    LV_IMG_DECLARE(exentec_logo);
    lv_obj_t * logo = lv_img_create(reminder_screen);
    lv_img_set_src(logo, &exentec_logo);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 140);

    lv_obj_t * contact_info = lv_label_create(reminder_screen);
    lv_label_set_text(contact_info, "Exentec Services Malaysia Sdn Bhd\n\n03-55422288 / 016-9704099");
    lv_obj_set_style_text_font(contact_info, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(contact_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(contact_info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(contact_info, LV_ALIGN_TOP_MID, 0, 280); // Moved down below larger logo
    
    lv_obj_t * btn = lv_btn_create(reminder_screen);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x404040), 0);
    
    lv_obj_t * lbl = lv_label_create(btn);
    
    if (is_preview) {
        lv_label_set_text(lbl, "CLOSE PREVIEW");
        lv_obj_add_event_cb(btn, preview_back_cb, LV_EVENT_CLICKED, NULL);
    } else {
        lv_label_set_text(lbl, "ACKNOWLEDGE");
        lv_obj_add_event_cb(btn, reminder_ack_cb, LV_EVENT_CLICKED, NULL);
    }
    
    lv_obj_center(lbl);
}

static void update_time_timer_cb(lv_timer_t * timer) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (time_label) {
        lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    
    // --- Service Reminder Logic (Once per Second Check is fine, barely any load) ---
    // Only check if time is valid (year > 2000)
    if ((timeinfo.tm_year + 1900) > 2000) {
        // Calculate Expiry Date
        struct tm expiry_tm = {0};
        expiry_tm.tm_year = service_config.cal_year - 1900;
        expiry_tm.tm_mon = service_config.cal_month - 1;
        expiry_tm.tm_mday = service_config.cal_day;
        
        // Add duration
        if (service_config.expiry_period == 0) expiry_tm.tm_mon += 6; // 6 Months
        else expiry_tm.tm_year += 1; // 1 Year
        
        // Normalize
        time_t expiry_time = mktime(&expiry_tm);
        
        // Diff: expiry - now
        double diff_seconds = difftime(expiry_time, now);
        
        // 7 days in seconds = 7 * 24 * 3600 = 604800
        if (diff_seconds <= 604800 && diff_seconds >= -864000) { // Warn if within 7 days or overdue (up to 10 days? forever? let's say forever until re-calibrated)
             // Check if already acked TODAY
             bool acked_today = (service_config.last_ack_year == (timeinfo.tm_year + 1900) && 
                                 service_config.last_ack_month == (timeinfo.tm_mon + 1) && 
                                 service_config.last_ack_day == timeinfo.tm_mday);
                                 
             if (!acked_today && lv_scr_act() != reminder_screen) {
                 // Trigger Reminder
                 last_active_screen = lv_scr_act();
                 create_reminder_screen(false); // Normal mode
                 lv_scr_load(reminder_screen);
             }
        }
    }
    
    // --- 1-Minute Trend Update ---
    static int last_min = -1;
    if (timeinfo.tm_min != last_min) {
        last_min = timeinfo.tm_min;
        
        // Add current value for ALL gauges
        // Note: Using current instantaneous value. Could average but raw is fine per spec.
        for(int i=0; i<16; i++) {
             trend_manager_add_point(i, sys_modbus_data.analog_vals[i]);
        }
        
        // --- 12-Hour Save Persistence ---
        // Save at 00:00 and 12:00 to minimize flash wear
        if (timeinfo.tm_min == 0 && (timeinfo.tm_hour % 12 == 0)) {
            trend_manager_save_to_nvs();
        }
    }
    
    if (wifi_status_icon) {
        if (wifi_connected) {
            lv_obj_set_style_text_color(wifi_status_icon, lv_color_hex(0x00AA00), 0); // Green
        } else {
            lv_obj_set_style_text_color(wifi_status_icon, lv_color_hex(0xAA0000), 0); // Red
        }
    }
    
    // Update System Info in Settings if created
    if (sys_wifi_label && lv_obj_is_valid(sys_wifi_label)) {
        if (wifi_connected) {
            lv_label_set_text(sys_wifi_label, "WiFi Status: Connected");
            lv_obj_set_style_text_color(sys_wifi_label, lv_color_hex(0x00FF00), 0);
        } else {
            lv_label_set_text(sys_wifi_label, "WiFi Status: Disconnected");
            lv_obj_set_style_text_color(sys_wifi_label, lv_color_hex(0xFF0000), 0);
        }
    }
    
    // Update Tab 5 Status Label
    if (lbl_wifi_status && lv_obj_is_valid(lbl_wifi_status)) {
        if (wifi_connected) {
            lv_label_set_text_fmt(lbl_wifi_status, "Status: Connected (%s)", system_ip_str);
            lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x00FF00), 0);
        } else {
            lv_label_set_text(lbl_wifi_status, "Status: Disconnected");
            lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF0000), 0);
        }
    }
    
    if (sys_ip_label && lv_obj_is_valid(sys_ip_label)) {
        lv_label_set_text_fmt(sys_ip_label, "IP Address: %s", system_ip_str);
    }
    
    // Update MQTT Status Label
    // Assuming mqtt_connected is accessible (it's static in publisher, need accessor or external)
    // For now, I'll assume I can add a getter to mqtt_publisher.h or make it extern.
    // Let's add `extern bool mqtt_is_connected(void);` to header and use it.
    if (sys_mqtt_label && lv_obj_is_valid(sys_mqtt_label)) {
        if (mqtt_is_connected()) {
            lv_label_set_text(sys_mqtt_label, "MQTT Status: Connected");
            lv_obj_set_style_text_color(sys_mqtt_label, lv_color_hex(0x00FF00), 0);
        } else {
            lv_label_set_text(sys_mqtt_label, "MQTT Status: Disconnected");
            lv_obj_set_style_text_color(sys_mqtt_label, lv_color_hex(0xFF0000), 0);
        }
    }
}

static void service_btn_event_cb(lv_event_t * e) {
    create_pin_screen();
    lv_scr_load(pin_screen);
}

static void create_main_screen(void) {
    if(lvgl_port_lock(-1)) {
        main_screen = lv_obj_create(NULL);
        // Set screen background to Dark Grey (Dark Theme)
        lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x202020), 0);
        
        // Time Label (Top Right)
        // Time Label (Moved to Top Left to make room for Logo)
        time_label = lv_label_create(main_screen);
        lv_label_set_text(time_label, "--:--:--");
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 20, 10); 

        // Modbus Init
        ESP_LOGI(TAG, "Initializing Modbus Master...");
        if (modbus_master_init() != ESP_OK) {
            ESP_LOGE(TAG, "Modbus Init Failed");
        }
        
        // Initialize Warning Screen (Hidden until needed)
        create_warning_screen();

        bsp_display_backlight_on();
        // WiFi Icon (Right of Time)
        wifi_status_icon = lv_label_create(main_screen);
        lv_label_set_text(wifi_status_icon, LV_SYMBOL_WIFI); 
        lv_obj_set_style_text_color(wifi_status_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align_to(wifi_status_icon, time_label, LV_ALIGN_OUT_RIGHT_MID, 30, 0);
        
        // Modbus Status Label (Top Center)
        // Create Title Header
        lv_obj_t * title_header = lv_label_create(main_screen);
        lv_label_set_text(title_header, "SF6 GAS LEAK MONITORING (WAFER PROBE 1)");
        lv_obj_set_style_text_font(title_header, &lv_font_montserrat_36, 0); // Bold/Large
        lv_obj_set_style_text_color(title_header, lv_color_hex(0xFFFFFF), 0); // White
        lv_obj_set_style_text_color(title_header, lv_color_hex(0xFFFFFF), 0); // White
        lv_obj_align(title_header, LV_ALIGN_TOP_MID, 0, 10); // Top Center

        // Add Dashboard Icon (Top Right)
        lv_obj_t * icon_img = lv_image_create(main_screen);
        lv_image_set_src(icon_img, &logo_img);
        lv_obj_align(icon_img, LV_ALIGN_TOP_RIGHT, -20, 5); // Top Right with padding

        // Modbus Status Label (Moved to Bottom)
        mb_status_label = lv_label_create(main_screen);
        if(mb_status_label) ESP_LOGI(TAG, "mb_status_label created: %p", mb_status_label);
        else ESP_LOGE(TAG, "Failed to create mb_status_label");
        
        lv_label_set_text(mb_status_label, "MB: INIT"); 
        lv_obj_set_style_text_font(mb_status_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(mb_status_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(mb_status_label, LV_ALIGN_BOTTOM_MID, 0, -10); // Bottom Center


        // --- Grid Layout ---
        // Page 1 Container
        grid_page_1 = lv_obj_create(main_screen);
        lv_obj_set_size(grid_page_1, 1140, 720); 
        lv_obj_align(grid_page_1, LV_ALIGN_CENTER, 0, 20); 
        lv_obj_set_style_bg_opa(grid_page_1, 0, 0);
        lv_obj_set_style_border_width(grid_page_1, 0, 0);
        lv_obj_set_flex_flow(grid_page_1, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grid_page_1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN); 
        lv_obj_set_style_pad_column(grid_page_1, 10, 0);
        lv_obj_set_style_pad_row(grid_page_1, 10, 0);

        // Populate Page 1 (Gauges 0-7)
        for(int i=0; i<8; i++) {
            create_gas_widget(grid_page_1, i);
        }
        
        // Page 2 Container (Initially Hidden)
        grid_page_2 = lv_obj_create(main_screen);
        lv_obj_set_size(grid_page_2, 1140, 720); 
        lv_obj_align(grid_page_2, LV_ALIGN_CENTER, 0, 20); 
        lv_obj_set_style_bg_opa(grid_page_2, 0, 0);
        lv_obj_set_style_border_width(grid_page_2, 0, 0);
        lv_obj_set_flex_flow(grid_page_2, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grid_page_2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN); 
        lv_obj_set_style_pad_column(grid_page_2, 10, 0);
        lv_obj_set_style_pad_row(grid_page_2, 10, 0);
        lv_obj_add_flag(grid_page_2, LV_OBJ_FLAG_HIDDEN); // Start hidden

        // Populate Page 2 (Gauges 8-15)
        for(int i=8; i<16; i++) {
            create_gas_widget(grid_page_2, i);
        }
        
        // Page 3 Container (Relay Monitor) - Initially Hidden
        grid_page_3 = lv_obj_create(main_screen);
        if(grid_page_3) ESP_LOGI(TAG, "grid_page_3 created: %p", grid_page_3);
        else ESP_LOGE(TAG, "Failed to create grid_page_3");
        lv_obj_set_size(grid_page_3, 1140, 720); 
        lv_obj_align(grid_page_3, LV_ALIGN_CENTER, 0, 20); 
        lv_obj_set_style_bg_opa(grid_page_3, 0, 0);
        lv_obj_set_style_border_width(grid_page_3, 0, 0);
        lv_obj_set_flex_flow(grid_page_3, LV_FLEX_FLOW_ROW_WRAP);
        // Align closer to top-left or center? Center is fine.
        lv_obj_set_flex_align(grid_page_3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START); 
        lv_obj_add_flag(grid_page_3, LV_OBJ_FLAG_HIDDEN);

        // -- Construct Page 3 Content --
        
        // Relays Header
        lv_obj_t * r_hdr = lv_label_create(grid_page_3);
        lv_label_set_text(r_hdr, "Relay Modules (16 Channels) - Read Only");
        lv_obj_set_style_text_font(r_hdr, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(r_hdr, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_width(r_hdr, 1100); // Full width break
        lv_obj_set_style_pad_bottom(r_hdr, 10, 0);

        // Relay Grid
        lv_obj_t * relay_cont = lv_obj_create(grid_page_3);
        lv_obj_set_size(relay_cont, 1100, 300);
        lv_obj_set_style_bg_color(relay_cont, lv_color_hex(0x101010), 0);
        lv_obj_set_flex_flow(relay_cont, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(relay_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        
        for(int i=0; i<16; i++) {
            lv_obj_t * item = lv_obj_create(relay_cont);
            lv_obj_set_size(item, 130, 60);
            lv_obj_set_style_bg_color(item, lv_color_hex(0x303030), 0);
            lv_obj_set_style_pad_all(item, 5, 0);
            
            lv_obj_t * lbl = lv_label_create(item);
            lv_label_set_text_fmt(lbl, "R%d", i+1);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 5, 0);
            
            // LED
            lv_obj_t * led = lv_led_create(item);
            lv_obj_set_size(led, 20, 20);
            lv_obj_align(led, LV_ALIGN_RIGHT_MID, -5, 0);
            lv_led_off(led);
            lv_led_set_color(led, lv_color_hex(0x00FF00));
            
            relay_leds[i] = led;
        }
        
        // Buttons Header
        lv_obj_t * b_hdr = lv_label_create(grid_page_3);
        lv_label_set_text(b_hdr, "Digital Input Buttons (4 Channels)");
        lv_obj_set_style_text_font(b_hdr, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(b_hdr, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_width(b_hdr, 1100);
        lv_obj_set_style_pad_top(b_hdr, 30, 0);
        lv_obj_set_style_pad_bottom(b_hdr, 10, 0);
        

        // --- BUTTONS & OVERLAYS (Must be created LAST for Z-Order) ---

        // Service Button (Bottom Left)
        // Service Button (Bottom Left)
        lv_obj_t * service_btn = lv_btn_create(main_screen);
        lv_obj_set_size(service_btn, 40, 40); 
        lv_obj_align(service_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
        lv_obj_add_event_cb(service_btn, service_btn_event_cb, LV_EVENT_CLICKED, (void*)0); 
        
        lv_obj_t * service_lbl = lv_label_create(service_btn);
        lv_label_set_text(service_lbl, LV_SYMBOL_EDIT); 
        lv_obj_set_style_text_color(service_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(service_lbl);

        // Settings Button (Bottom Right)
        lv_obj_t * settings_btn = lv_btn_create(main_screen);
        lv_obj_set_size(settings_btn, 40, 40); 
        lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
        lv_obj_add_event_cb(settings_btn, settings_btn_event_cb, LV_EVENT_CLICKED, (void*)0); 
        
        lv_obj_t * settings_lbl = lv_label_create(settings_btn);
        lv_label_set_text(settings_lbl, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_color(settings_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(settings_lbl);
        
        // Start Update Timer
        lv_timer_create(gas_update_timer_cb, 100, NULL);
        lv_timer_create(update_time_timer_cb, 1000, NULL);

        // Navigation Buttons (Create Here to be on Top)
        btn_next = lv_btn_create(main_screen);
        lv_obj_set_size(btn_next, 60, 60);
        lv_obj_align(btn_next, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x202020), 0);
        lv_obj_set_style_radius(btn_next, 30, 0);
        lv_obj_add_event_cb(btn_next, next_page_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lbl_next = lv_label_create(btn_next);
        lv_label_set_text(lbl_next, LV_SYMBOL_RIGHT);
        lv_obj_center(lbl_next);

        btn_prev = lv_btn_create(main_screen);
        lv_obj_set_size(btn_prev, 60, 60);
        lv_obj_align(btn_prev, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x202020), 0);
        lv_obj_set_style_radius(btn_prev, 30, 0);
        lv_obj_add_event_cb(btn_prev, prev_page_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(btn_prev, LV_OBJ_FLAG_HIDDEN); // Start hidden
        
        lv_obj_t * lbl_prev = lv_label_create(btn_prev);
        lv_label_set_text(lbl_prev, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_prev);

        // Bring labels to top as well
        if (time_label) lv_obj_move_foreground(time_label);
        if (wifi_status_icon) lv_obj_move_foreground(wifi_status_icon);
        if (mb_status_label) lv_obj_move_foreground(mb_status_label);

        // Ensure containers don't block input (Clickthrough)
        lv_obj_clear_flag(grid_page_1, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(grid_page_2, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(grid_page_3, LV_OBJ_FLAG_CLICKABLE);

        lv_scr_load(main_screen);
        lvgl_port_unlock();
    }
}
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Init Logging (PSRAM)
    // init_logging(); // Assuming this is defined elsewhere if needed
    
    // Init Configs (Load from NVS)
    load_gauge_configs();
    
    // Init Trending (mounts SPIFFS, allocates PSRAM)
    trend_manager_init();

    // Load WiFi Creds (After NVS init)
    load_wifi_creds();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // msg_print_all_logs(); // Removed: Invalid function

    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_pass, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialization complete");

    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized");

    bsp_display_brightness_init();

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);

    // Initialize RTC (shares I2C bus)
    rx8025t_init(i2c_handle);
    
    // Set Timezone to Kuala Lumpur (GMT+8)
    // POSIX format: "MYT-8" (standard-offset, so -8 means GMT+8)
    setenv("TZ", "MYT-8", 1);
    tzset();
    
    // Try to load time from RTC on boot
    struct tm rtc_time;
    if (rx8025t_get_time(&rtc_time) == ESP_OK) {
        // RTC read successful
        if (rtc_time.tm_year > (2020 - 1900)) { // Simple validity check
            time_t t = mktime(&rtc_time);
            struct timeval now = { .tv_sec = t };
            settimeofday(&now, NULL);
            ESP_LOGI(TAG, "System time set from RTC: %s", asctime(&rtc_time));
        } else {
            ESP_LOGW(TAG, "RTC time invalid, skipping system set");
        }
    } else {
        ESP_LOGW(TAG, "Failed to read RTC on boot");
    }

    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();

    esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

     ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_config =JD9365_PANEL_IO_DBI_CONFIG();

    esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);

    esp_lcd_panel_handle_t disp_panel = NULL;
    ESP_LOGI(TAG, "Install LCD driver of st7701");
    esp_lcd_dpi_panel_config_t dpi_config ={
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,  
        .dpi_clock_freq_mhz = 63,                     
        .virtual_channel = 0,                         
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,                    
        .num_fbs = LVGL_PORT_LCD_BUFFER_NUMS,                                 
        .video_timing = {                             
            .h_size = 800,                            
            .v_size = 1280,                            
            .hsync_back_porch = 20,                   
            .hsync_pulse_width = 20,                  
            .hsync_front_porch = 40,                  
            .vsync_back_porch = 8,                      
            .vsync_pulse_width = 4,                     
            .vsync_front_porch = 20,                  
        },                                            
        .flags.use_dma2d = true,                      
    };

    jd9365_vendor_config_t vendor_config = {
        .init_cmds = lcd_cmd,
        .init_cmds_size = sizeof(lcd_cmd) / sizeof(jd9365_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_27,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io, &panel_config, &disp_panel));
    esp_lcd_panel_reset(disp_panel);
    esp_lcd_panel_init(disp_panel);

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
#if LVGL_PORT_AVOID_TEAR_MODE
        .on_refresh_done = mipi_dsi_lcd_on_vsync_event,
#else
        .on_color_trans_done = mipi_dsi_lcd_on_vsync_event,
#endif
    };
    esp_lcd_dpi_panel_register_event_callbacks(disp_panel, &cbs, NULL);
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
    // tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);

     const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES, // Swapped for Landscape (1280)
        .y_max = BSP_LCD_H_RES, // Swapped for Landscape (800)
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        // .process_coordinates = touch_correction, // Removed manual callback
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting app_main...");
    
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gsl3680(tp_io_handle, &tp_cfg, &tp_handle));

    lvgl_port_interface_t interface = (dpi_config.flags.use_dma2d) ? LVGL_PORT_INTERFACE_MIPI_DSI_DMA : LVGL_PORT_INTERFACE_MIPI_DSI_NO_DMA;
    ESP_LOGI(TAG,"interface is %d",interface);
    ESP_ERROR_CHECK(lvgl_port_init(disp_panel, tp_handle, interface));

    bsp_display_backlight_on();

    // Start MQTT (5 second interval)
    mqtt_app_start();
    lv_timer_create(mqtt_timer_cb, 5000, NULL);

    // Show Main Screen (Gas Widget)
    create_main_screen();
}
