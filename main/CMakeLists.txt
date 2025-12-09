idf_component_register(SRCS "lvgl_sw_rotation.c" "lvgl_port_v9.c" "rx8025t.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lcd esp_timer lvgl driver nvs_flash esp_wifi esp_netif esp_driver_gpio esp_driver_i2c esp_driver_ppa esp_lcd_touch esp_lcd_touch_gsl3680 esp_lcd_jd9365 esp_wifi_remote esp_mm lwip)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)