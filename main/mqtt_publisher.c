#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_publisher.h"

static const char *TAG = "MQTT_PUB";

// Embedded CA Cert
extern const uint8_t ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t ca_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

bool mqtt_is_connected(void) {
    return mqtt_connected;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, (long)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    // int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://a0c56800.ala.asia-southeast1.emqxsl.com:8883",
        .broker.verification.certificate = (const char *)ca_cert_pem_start,
        .credentials.username = "arktech",
        .credentials.authentication.password = "arktech",
        .credentials.client_id = "unisem",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT Client Started");
}

void mqtt_publish_gauge_data(float *values, int count)
{
    if (!client || !mqtt_connected) {
        ESP_LOGW(TAG, "Cannot publish: MQTT not connected");
        return;
    }

    if (count < 16) return;

    char payload[512]; // Increased buffer
    // Format: {"device": "esp32-01", "ai": {"ch1": ..., "ch16": ...}}
    int offset = snprintf(payload, sizeof(payload), "{\"device\": \"esp32-01\", \"ai\": {");
    
    for (int i = 0; i < 16; i++) {
        offset += snprintf(payload + offset, sizeof(payload) - offset, "\"ch%d\": %.2f%s", 
            i + 1, values[i], (i < 15) ? ", " : "");
        if (offset >= sizeof(payload)) break;
    }
    
    snprintf(payload + offset, sizeof(payload) - offset, "}}");

    int msg_id = esp_mqtt_client_publish(client, "unisem", payload, 0, 1, 0);
    ESP_LOGI(TAG, "published %d bytes, msg_id=%d", offset, msg_id);
}
