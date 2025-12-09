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
#include "esp_sntp.h"
#include "esp_event.h"
#include "driver/spi_master.h"
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

static const char *TAG = "BarGauge";

// WiFi credentials
#define WIFI_SSID      "AKR Home"
#define WIFI_PASSWORD  "brandy78755862"

// WiFi status
static bool wifi_connected = false;
static char time_str[32] = "--:--:--";
static lv_obj_t *time_label = NULL; // Time display label

// Timer callback to update time display
static void update_time_timer_cb(lv_timer_t *timer) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Only update if year is valid (> 2020) to avoid showing epoch
    if (timeinfo.tm_year > (2020 - 1900)) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        if (time_label) {
            lv_label_set_text(time_label, time_str);
        }
    }
}


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

static esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

static esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

// --- Configuration Structure ---
typedef struct {
    char name[32];    // Gauge Name
    char unit[16];    // Gauge Unit (e.g. "PPM")
    int min_val;
    int max_val;
    int blue_limit;   // 0 to blue_limit (Cyan)
    int yellow_limit; // blue_limit to yellow_limit (Yellow)
    int red_limit;    // yellow_limit to red_limit (Warning?? No, usually Red starts here)
    int threshold;    // Future use
} GasGaugeConfig;

static GasGaugeConfig methane_config = {
    .name = "METHANE",
    .unit = "PPM",
    .min_val = 0,
    .max_val = 100,
    .blue_limit = 30, // 0-30 Cyan
    .yellow_limit = 70, // 30-70 Yellow
    .red_limit = 100,  // unused
    .threshold = 80
};

// Forward declarations
static lv_obj_t * create_gas_widget(lv_obj_t *parent);
static void create_trending_screen(void);
static void create_settings_screen(void);
static void create_main_screen(void);

// Global/Static references for updates
static lv_obj_t * const_arc = NULL;
static lv_obj_t * const_val_label = NULL;
static lv_obj_t * const_title_lbl = NULL; // To update name
static lv_obj_t * const_unit_lbl = NULL; // To update unit
static lv_obj_t * const_chart = NULL;
static lv_chart_series_t * const_ser1 = NULL;

static lv_obj_t * main_screen = NULL;
static lv_obj_t * trending_screen = NULL;
static lv_obj_t * settings_screen = NULL;
static lv_obj_t * kb = NULL; // Global keyboard

static void trending_btn_event_cb(lv_event_t * e) {
    if (trending_screen == NULL) {
        create_trending_screen();
    }
    lv_scr_load_anim(trending_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, false);
}

static void settings_btn_event_cb(lv_event_t * e) {
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
        if (const_title_lbl) {
            lv_label_set_text(const_title_lbl, methane_config.name);
        }
        // Update Unit if changed
        if (const_unit_lbl) {
            lv_label_set_text(const_unit_lbl, methane_config.unit);
        }
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
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(ta);
        snprintf(methane_config.name, sizeof(methane_config.name), "%s", txt);
    }
    ta_event_cb(e); // Chain to standard handler
}

static void unit_ta_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(ta);
        snprintf(methane_config.unit, sizeof(methane_config.unit), "%s", txt);
    }
    ta_event_cb(e); // Chain to standard handler
}

static void int_val_ta_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * ta = lv_event_get_target(e);
        /* Identify which field to update by user_data pointer to int* */
        int * target_ptr = (int*)lv_event_get_user_data(e);
        
        const char * txt = lv_textarea_get_text(ta);
        if (target_ptr) {
            *target_ptr = atoi(txt);
        }
    }
    
    // Manual Keyboard Logic since we hijacked user_data
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
static void create_config_row(lv_obj_t * parent, const char * title, char * text_buffer, int * int_ptr, int type) {
    // Type 0: Int, 1: Name, 2: Unit
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 780, 70); // Wider, taller
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0); // Large font
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t * ta = lv_textarea_create(cont);
    lv_obj_set_size(ta, 300, 50);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0); // Large font
    lv_textarea_set_one_line(ta, true);
    
    if (type == 1) { // Name
        lv_textarea_set_text(ta, text_buffer);
        lv_textarea_set_max_length(ta, 31);
        lv_obj_add_event_cb(ta, name_ta_cb, LV_EVENT_ALL, (void*)1); 
    } else if (type == 2) { // Unit
        lv_textarea_set_text(ta, text_buffer);
        lv_textarea_set_max_length(ta, 15);
        lv_obj_add_event_cb(ta, unit_ta_cb, LV_EVENT_ALL, (void*)1); 
    } else { // Int
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", *int_ptr);
        lv_textarea_set_text(ta, buf);
        lv_textarea_set_max_length(ta, 4); // Max 4 digits
        // Pass int pointer as User Data
        lv_obj_add_event_cb(ta, int_val_ta_cb, LV_EVENT_ALL, (void*)int_ptr); 
    }
}

static void create_settings_screen(void) {
    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(settings_screen, LV_FLEX_FLOW_COLUMN);
    // Align Start so we can scroll if needed, but centering horizontally
    lv_obj_set_flex_align(settings_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(settings_screen, 20, 0);

    // Header
    lv_obj_t * header = lv_label_create(settings_screen);
    lv_label_set_text(header, "Configuration");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_32, 0); // Larger Header
    lv_obj_set_style_text_color(header, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_bottom(header, 30, 0);

    // Rows
    create_config_row(settings_screen, "Gauge Name", methane_config.name, NULL, 1);
    create_config_row(settings_screen, "Gauge Unit", methane_config.unit, NULL, 2);
    create_config_row(settings_screen, "Min Value", NULL, &methane_config.min_val, 0);
    create_config_row(settings_screen, "Max Value", NULL, &methane_config.max_val, 0);
    create_config_row(settings_screen, "Blue Limit", NULL, &methane_config.blue_limit, 0);
    create_config_row(settings_screen, "Yellow Limit", NULL, &methane_config.yellow_limit, 0);

    // Back Button (At bottom of scroll)
    lv_obj_t * btn = lv_btn_create(settings_screen);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_add_event_cb(btn, back_from_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x505050), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_margin_top(btn, 20, 0);
    
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_UP " Back");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
    
    // Keyboard (Create last so it's on top)
    kb = lv_keyboard_create(settings_screen);
    lv_obj_set_size(kb, 800, 300); // Half Screen
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0); // Stick to bottom
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // Hide initially
}

static lv_obj_t* create_gas_widget(lv_obj_t *parent) {
    // 1. Container (Dark Background)
    lv_obj_t * container = lv_obj_create(parent);
    lv_obj_set_size(container, 400, 400);
    lv_obj_center(container);
    lv_obj_set_style_bg_opa(container, 0, 0); // Transparent
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    // 2. Arc Gauge (270 degree)
    lv_obj_t * arc = lv_arc_create(container);
    lv_obj_set_size(arc, 300, 300);
    lv_arc_set_rotation(arc, 135); // Start at bottom-left
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, methane_config.min_val, methane_config.max_val);
    lv_arc_set_value(arc, 0);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB); // Remove knob
    
    // Style: Dark Grey Background Arc
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_MAIN);
    
    // Style: Indicator Arc
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR); // Cyan default
    lv_obj_set_style_arc_width(arc, 20, LV_PART_INDICATOR);
    
    const_arc = arc;

    // 3. Digital Value "00"
    lv_obj_t * val_lbl = lv_label_create(container);
    lv_label_set_text(val_lbl, "0");
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(val_lbl);
    lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, -10);
    
    const_val_label = val_lbl;

    // 4. Label "METHANE"
    lv_obj_t * title_lbl = lv_label_create(container);
    lv_label_set_text(title_lbl, methane_config.name); // Dynamic Name
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(title_lbl, LV_ALIGN_CENTER, 0, -50);
    
    const_title_lbl = title_lbl;

    // 5. Label "PPM"
    lv_obj_t * unit_lbl = lv_label_create(container);
    lv_label_set_text(unit_lbl, methane_config.unit); // Dynamic Unit
    lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, 30);
    
    const_unit_lbl = unit_lbl;

    // 6. Trending Button
    lv_obj_t * btn = lv_btn_create(container);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x505050), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_add_event_cb(btn, trending_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "TRENDING");
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);

    return container;
}

static void create_trending_screen(void) {
    trending_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(trending_screen, lv_color_hex(0x000000), 0); // Black bg

    lv_obj_t * label = lv_label_create(trending_screen);
    lv_label_set_text(label, "Trending Data");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t * back_btn = lv_btn_create(trending_screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0x505050), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_add_event_cb(back_btn, back_from_trending_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl = lv_label_create(back_btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);

    // Chart Implementation
    lv_obj_t * chart = lv_chart_create(trending_screen);
    lv_obj_set_size(chart, 600, 300); // Landscape: 800 width, use 600.
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 50); // Show last 50 points
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100); // Should also likely depend on config?
    // For now hardcoded 0-100 in chart creation but we can update it in timer too.
    
    // Style: Dark Chart Background
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x101010), 0); // Slightly lighter than black
    lv_obj_set_style_border_color(chart, lv_color_hex(0x404040), 0);
    
    // Style: Grid lines
    lv_obj_set_style_line_color(chart, lv_color_hex(0x303030), LV_PART_MAIN); // Grid
    
    // Series: Cyan
    const_ser1 = lv_chart_add_series(chart, lv_color_hex(0x00E0FF), LV_CHART_AXIS_PRIMARY_Y);
    const_chart = chart;
}

// Timer to simulate data updates and apply config
static void gas_update_timer_cb(lv_timer_t * timer) {
    // Simulate value 0-100
    static float val = 0;
    static bool up = true;
    if(up) val += 1.0f; else val -= 1.0f;
    if(val >= 100) up = false;
    if(val <= 0) up = true;
    
    // Update Arc
    if(const_arc) {
        // Apply Config Range dynamically
        lv_arc_set_range(const_arc, methane_config.min_val, methane_config.max_val);
        lv_arc_set_value(const_arc, (int32_t)val);
        
        // Dynamic Color Logic based on Config
        // Blue Zone: [0, blue_limit]
        // Yellow Zone: [blue_limit, yellow_limit]
        // Red Zone: [yellow_limit, max]
        
        if (val < methane_config.blue_limit) {
            // Blue/Cyan
           lv_obj_set_style_arc_color(const_arc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR); 
        } else if (val < methane_config.yellow_limit) {
            // Yellow
           lv_obj_set_style_arc_color(const_arc, lv_color_hex(0xFFFF00), LV_PART_INDICATOR);
        } else {
            // Red (Danger)
           lv_obj_set_style_arc_color(const_arc, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
           lv_obj_set_style_img_recolor(const_arc, lv_color_hex(0xFF0000), LV_PART_KNOB); 
        }
    }

    // Update Label
    if(const_val_label) {
        lv_label_set_text_fmt(const_val_label, "%d", (int)val);
    }
}

static void create_main_screen(void) {
    if(lvgl_port_lock(-1)) {
        main_screen = lv_obj_create(NULL);
        // Set screen background to Black (VolosR style)
        lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
        
        // Time Label (Top Right or Center)
        time_label = lv_label_create(main_screen);
        lv_label_set_text(time_label, "--:--:--");
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(time_label, lv_color_hex(0x00BFFF), 0);
        lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -20, 20);
        
        // Settings Button (Bottom Right)
        lv_obj_t * settings_btn = lv_btn_create(main_screen);
        lv_obj_set_size(settings_btn, 50, 50);
        lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x202020), 0);
        lv_obj_set_style_shadow_width(settings_btn, 0, 0);
        lv_obj_add_event_cb(settings_btn, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * sett_lbl = lv_label_create(settings_btn);
        lv_label_set_text(sett_lbl, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_color(sett_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(sett_lbl);

        // Create Gas Widget
        create_gas_widget(main_screen);
        
        // Start Update Timer
        lv_timer_create(gas_update_timer_cb, 100, NULL);
        lv_timer_create(update_time_timer_cb, 1000, NULL);

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

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialization complete");

    // Initialize SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
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
    // Show Main Screen (Gas Widget)
    create_main_screen();
}
