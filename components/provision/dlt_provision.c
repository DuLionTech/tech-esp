#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include "dlt_provision.h"
#define OK ESP_ERROR_CHECK

// Private Declarations
static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ble_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Public Implementations
void create_wifi(void) {
    OK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provision_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &ble_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &session_handler, NULL));
}

// Private Implementations
static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}

static void ble_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}

static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}

