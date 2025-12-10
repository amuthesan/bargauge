idf_component_register(SRCS "lvgl_sw_rotation.c" "lvgl_port_v9.c" "rx8025t.c" "modbus_master.c" "mqtt_publisher.c"
                        INCLUDE_DIRS "."
                        REQUIRES nvs_flash esp_netif esp_wifi esp_lcd esp_lcd_touch lvgl esp_lcd_touch_gsl3680 esp_lcd_jd9365 esp_timer esp_driver_i2c driver esp_mm log freertos esp_system esp_driver_ppa spi_flash mqtt)

target_add_binary_data(${COMPONENT_LIB} "ca_cert.pem" TEXT)
