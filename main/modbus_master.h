#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Modbus Data Structure (Global)
typedef struct {
    uint16_t analog_vals[16];   // 0-7 from ID1, 8-15 from ID2
    bool relays[16];            // 0-7 from ID3, 8-15 from ID4
    bool buttons[4];            // 0-3 from ID4 (DI)
    bool connected[4];          // Connection status for each slave (ID 1-4)
} system_modbus_data_t;

extern system_modbus_data_t sys_modbus_data;

// Initialize Modbus Master
esp_err_t modbus_master_init(void);

// Task to poll devices (created internally by init, or exposed if needed)
// We'll let init create the task.

// Set Relay State (0-15)
esp_err_t modbus_set_relay(int index, bool state);
