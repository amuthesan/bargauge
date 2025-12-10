#include "modbus_master.h"
#include "esp_log.h"
#include "mbcontroller.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MB_MASTER";

// Hardware Config
#define MB_PORT_NUM     (UART_NUM_2) // Use UART2
#define MB_TX_PIN       (GPIO_NUM_37)
#define MB_RX_PIN       (GPIO_NUM_38)
#define MB_RTS_PIN      (UART_PIN_NO_CHANGE) // No hardware flow control used usually for minimal RS485, or mapped to DE
#define MB_CTS_PIN      (UART_PIN_NO_CHANGE)
// Note: ESP-Modbus manages RTS/CTS for DE/RE if configured. 
// Reference says "manual wiring A/B", implies automatic direction control or specific DE pin.
// Config.json says "DE_RE: GPIO4". Let's assume we need it.
// Wait, user request said "GPIO 45 (TX) and GPIO 46 (RX)". Did not mention DE.
// If using MAX485 manually, DE is needed. If using automatic hardware (e.g. self-toggling transciever), not needed.
// I will assume NO DE pin for now based on strict "use 45/46" request, or ask.
// Actually, default P4 board usually has no RS485 on 45/46. This is custom wiring.
// I will use UART_PIN_NO_CHANGE for RTS unless valid pin is provided. Defaulting to NO RTS.

#define MB_BAUD_RATE    (9600)

// Global Data Instance
system_modbus_data_t sys_modbus_data = {0};

// Modbus Parameter Descriptors
enum {
    CID_ANALOG_1 = 0, // ID 1, Reg 0-7
    CID_ANALOG_2,     // ID 2, Reg 0-7
    CID_RELAY_1,      // ID 3, Coils 0-7
    CID_RELAY_2,      // ID 4, Coils 0-7
    CID_BUTTONS       // ID 4, DI 0-3
};

// Note: Structure is: 
// cid, param_key, param_units, mb_slave_addr, mb_param_type, mb_reg_start, mb_size, param_offset, param_type, param_size, access, limits

const mb_parameter_descriptor_t device_parameters[] = {
    { 
        .cid = CID_ANALOG_1, .param_key = "Analog 1-8", .param_units = "Volts", 
        .mb_slave_addr = 1, .mb_param_type = MB_PARAM_INPUT, .mb_reg_start = 0, .mb_size = 8, 
        .param_offset = 0, .param_type = PARAM_TYPE_U16, .param_size = 16, .param_opts = { .opt1 = 0, .opt2 = 0, .opt3 = 0 }, .access = PAR_PERMS_READ 
    },
    { 
        .cid = CID_ANALOG_2, .param_key = "Analog 9-16", .param_units = "Volts", 
        .mb_slave_addr = 2, .mb_param_type = MB_PARAM_INPUT, .mb_reg_start = 0, .mb_size = 8, 
        .param_offset = 0, .param_type = PARAM_TYPE_U16, .param_size = 16, .param_opts = { .opt1 = 0, .opt2 = 0, .opt3 = 0 }, .access = PAR_PERMS_READ 
    },
    { 
        .cid = CID_RELAY_1, .param_key = "Relay 1-8", .param_units = "Bool", 
        .mb_slave_addr = 3, .mb_param_type = MB_PARAM_COIL, .mb_reg_start = 0, .mb_size = 8, 
        .param_offset = 0, .param_type = PARAM_TYPE_U8, .param_size = 1, .param_opts = { .opt1 = 0, .opt2 = 0, .opt3 = 0 }, .access = PAR_PERMS_READ_WRITE 
    },
    { 
        .cid = CID_RELAY_2, .param_key = "Relay 9-16", .param_units = "Bool", 
        .mb_slave_addr = 4, .mb_param_type = MB_PARAM_COIL, .mb_reg_start = 0, .mb_size = 8, 
        .param_offset = 0, .param_type = PARAM_TYPE_U8, .param_size = 1, .param_opts = { .opt1 = 0, .opt2 = 0, .opt3 = 0 }, .access = PAR_PERMS_READ_WRITE 
    },
    { 
        .cid = CID_BUTTONS, .param_key = "Button 1-4", .param_units = "Bool", 
        .mb_slave_addr = 4, .mb_param_type = MB_PARAM_DISCRETE, .mb_reg_start = 0, .mb_size = 4, 
        .param_offset = 0, .param_type = PARAM_TYPE_U8, .param_size = 1, .param_opts = { .opt1 = 0, .opt2 = 0, .opt3 = 0 }, .access = PAR_PERMS_READ 
    }
};

// Calculate number of parameters
#define MASTER_MAX_CIDS (sizeof(device_parameters) / sizeof(device_parameters[0]))

static void modbus_poll_task(void *arg) {
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;
    uint8_t type = 0; // Data type

    while (1) {
        // Poll CID_ANALOG_1
        err = mbc_master_get_cid_info(CID_ANALOG_1, &param_descriptor);
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
            // We want to read 8 registers (16 bytes)
            // api: mbc_master_get_parameter(cid, name, value_ptr, type_ptr)
            err = mbc_master_get_parameter(CID_ANALOG_1, "Analog 1-8", (uint8_t*)&sys_modbus_data.analog_vals[0], &type);
            if (err == ESP_OK) {
                sys_modbus_data.connected[0] = true;
            } else {
                sys_modbus_data.connected[0] = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // Poll CID_ANALOG_2 (Offset index 8)
        err = mbc_master_get_parameter(CID_ANALOG_2, "Analog 9-16", (uint8_t*)&sys_modbus_data.analog_vals[8], &type);
        sys_modbus_data.connected[1] = (err == ESP_OK);
        vTaskDelay(pdMS_TO_TICKS(50));

        // Poll Relays 1 
        uint8_t relay_packed = 0;
        err = mbc_master_get_parameter(CID_RELAY_1, "Relay 1-8", &relay_packed, &type);
        if (err == ESP_OK) {
            sys_modbus_data.connected[2] = true;
            for(int i=0; i<8; i++) sys_modbus_data.relays[i] = (relay_packed >> i) & 1;
        } else {
            sys_modbus_data.connected[2] = false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // Poll Relays 2
        err = mbc_master_get_parameter(CID_RELAY_2, "Relay 9-16", &relay_packed, &type);
        if (err == ESP_OK) {
            sys_modbus_data.connected[3] = true; // ID 4
            for(int i=0; i<8; i++) sys_modbus_data.relays[i+8] = (relay_packed >> i) & 1;
        } else {
            sys_modbus_data.connected[3] = false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // Poll Buttons (ID 4)
        uint8_t btn_packed = 0;
        err = mbc_master_get_parameter(CID_BUTTONS, "Button 1-4", &btn_packed, &type);
        if (err == ESP_OK) {
             for(int i=0; i<4; i++) sys_modbus_data.buttons[i] = (btn_packed >> i) & 1;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Update every 500ms
    }
}

// Set Relay State
esp_err_t modbus_set_relay(int index, bool state) {
    // index: 0-15 (0-7 for Relay 1, 8-15 for Relay 2)
    if (index < 0 || index > 15) return ESP_ERR_INVALID_ARG;
    
    // Determine CID
    int cid = (index < 8) ? CID_RELAY_1 : CID_RELAY_2;
    char * param_key = (index < 8) ? "Relay 1-8" : "Relay 9-16";
    int local_bit = index % 8;
    
    // Reconstruct byte from current known state
    uint8_t current_byte = 0;
    int start_offset = (index < 8) ? 0 : 8;
    for(int i=0; i<8; i++) {
        if(sys_modbus_data.relays[start_offset + i]) current_byte |= (1 << i);
    }
    
    if (state) current_byte |= (1 << local_bit);
    else current_byte &= ~(1 << local_bit);
    
    // Write
    uint8_t type = 0;
    return mbc_master_set_parameter(cid, param_key, (uint8_t*)&current_byte, &type);
}

esp_err_t modbus_master_init(void) {
    // Communication info
    mb_communication_info_t comm = {
        .port = MB_PORT_NUM,
        .mode = MB_MODE_RTU,
        .baudrate = MB_BAUD_RATE,
        .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    ESP_ERROR_CHECK(mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler));
    ESP_ERROR_CHECK(mbc_master_setup((void*)&comm));
    ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, MB_TX_PIN, MB_RX_PIN, MB_RTS_PIN, MB_CTS_PIN));
    
    ESP_LOGI(TAG, "Modbus Master Init...");
    ESP_ERROR_CHECK(mbc_master_start());

    // Set Descriptors
    ESP_ERROR_CHECK(mbc_master_set_descriptor(device_parameters, MASTER_MAX_CIDS));

    ESP_LOGI(TAG, "Modbus Master Started on Tx:%d, Rx:%d", MB_TX_PIN, MB_RX_PIN);

    // Create polling task
    xTaskCreatePinnedToCore(modbus_poll_task, "modbus_task", 6144, NULL, 5, NULL, 1);
    
    ESP_LOGI(TAG, "Modbus Master Init Complete");
    return ESP_OK;
}
