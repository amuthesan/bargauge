#include "arduino_iot_bridge.h"
#include "Arduino.h"
#include "ArduinoIoTCloud.h"
#include "Arduino_ConnectionHandler.h"
#include "esp_log.h"

// --- Configuration ---
const char DEVICE_LOGIN_NAME[]  = "c2a8708d-172d-4447-a772-8d7ede5cbd0d";
const char DEVICE_KEY[]         = "E!iw8hwMtZC6fwy3xQSi4eFfC";
const char SSID[]               = "AKR Home";
const char PASS[]               = "brandy78755862";

static const char *TAG = "ArduinoIoT";

// --- Cloud Variables ---
float cH1;
float cH10; // Included but unused?
float cH2;
float cH3;
float cH4;
float cH5;
float cH6;
float cH7;
float cH8;
float cH9; // Included but unused?

// --- Connection Handler ---
WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

// --- Property Init ---
void initProperties() {
    ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
    ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
    
    // Read-only properties (Cloud reads from Device)
    // Permission: READ (Device sends to Cloud), Policy: ON_CHANGE
    ArduinoCloud.addProperty(cH1, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH2, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH3, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH4, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH5, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH6, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH7, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH8, READ, ON_CHANGE, NULL);
    
    // Extra ones requested
    ArduinoCloud.addProperty(cH9, READ, ON_CHANGE, NULL);
    ArduinoCloud.addProperty(cH10, READ, ON_CHANGE, NULL);
}

// --- Bridge Implementation ---

void arduino_iot_init(void) {
    ESP_LOGI(TAG, "Initializing Arduino Core...");
    initArduino();
    
    ESP_LOGI(TAG, "Initializing Arduino IoT Properties...");
    initProperties();
    
    ESP_LOGI(TAG, "Starting Arduino IoT Cloud...");
    ArduinoCloud.begin(ArduinoIoTPreferredConnection);
    
    setDebugMessageLevel(2);
    ArduinoCloud.printDebugInfo();
}

void arduino_iot_update_gauge(int index, float val) {
    switch(index) {
        case 0: cH1 = val; break;
        case 1: cH2 = val; break;
        case 2: cH3 = val; break;
        case 3: cH4 = val; break;
        case 4: cH5 = val; break;
        case 5: cH6 = val; break;
        case 6: cH7 = val; break;
        case 7: cH8 = val; break;
        // Map 8-9 if desired?
        case 8: cH9 = val; break;
        case 9: cH10 = val; break;
        default: break;
    }
}

void arduino_iot_task(void *pvParameters) {
    ESP_LOGI(TAG, "Arduino IoT Task Started");
    
    arduino_iot_init();
    
    while(1) {
        ArduinoCloud.update();
        // Small delay to yield
        delay(100); 
    }
}
