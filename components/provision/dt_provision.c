#include <string.h>
#include <dt_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_netif.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include <dt_provision.h>

#include "esp_check.h"

static const char* TAG = "dt_provision";
static esp_netif_t* s_wifi_netif = NULL;
static wifi_netif_driver_t s_wifi_driver = NULL;

// Private Declarations
static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void bluetooth_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void on_wifi_event(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Public Implementations
esp_err_t dt_provision_init(EventGroupHandle_t netif_event_group) {
    esp_err_t ret = ESP_FAIL;
    OK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provision_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &bluetooth_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &session_handler, NULL));
    OK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL));

    ESP_LOGI(TAG, "Initializing WiFi in station mode...");
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();
    s_wifi_netif = esp_netif_new(&netif_config);
    ESP_GOTO_ON_FALSE(s_wifi_netif != NULL, ESP_FAIL, fail, TAG, "Failed to create WiFi station interface");
    s_wifi_driver = esp_wifi_create_if_driver(WIFI_IF_STA);
    ESP_GOTO_ON_FALSE(s_wifi_driver != NULL, ESP_FAIL, fail, TAG, "Failed to create WiFi station driver");
    ESP_GOTO_ON_ERROR(esp_netif_attach(s_wifi_netif, s_wifi_driver), fail, TAG, "Failed to attach station driver");
    ESP_GOTO_ON_ERROR(
        esp_register_shutdown_handler((shutdown_handler_t) esp_wifi_stop),
        fail,
        TAG,
        "Failed to register shutdown handler");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_GOTO_ON_ERROR(esp_wifi_init(&cfg), fail, TAG, "Failed to initialize WiFi");
    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), fail, TAG, "Failed to set WiFi mode to station");

    // FIXME Continue here

    return ESP_OK;

fail:
    esp_unregister_shutdown_handler((shutdown_handler_t) esp_wifi_stop);

    esp_wifi_destroy_if_driver(s_wifi_driver);
    s_wifi_netif = NULL;

    esp_netif_destroy(s_wifi_netif);
    s_wifi_netif = NULL;

    return ret;
}

// Private Implementations
static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}

static void bluetooth_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}

static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
}
