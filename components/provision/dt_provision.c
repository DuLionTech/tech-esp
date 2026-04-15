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
#include <sys/types.h>

#include "esp_check.h"
#include "lwip/ip4_addr.h"
#include "lwip/priv/nd6_priv.h"

#define WIFI_CONN_ATTEMPTS 5

static const char* TAG = "dt_provision";
static esp_netif_t* s_wifi_netif = NULL;
static wifi_netif_driver_t s_wifi_driver = NULL;

// Private Declarations
static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void bluetooth_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void wifi_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t provision_data_handler(
    uint32_t session_id,
    const uint8_t* in_buf,
    ssize_t in_len,
    uint8_t** out_buf,
    ssize_t* out_len,
    void* data);

// asm("_binary_sec2_salt_start")
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};
// asm("_binary_sec2_salt_end")

// asm("_binary_sec2_verifier_start")
static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43, 0x78, 0xcf, 0xfd,
    0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24, 0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9,
    0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11, 0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a,
    0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89, 0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6,
    0xe4, 0x3a, 0x71, 0x2e, 0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87,
    0xdc, 0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4, 0x70, 0xad,
    0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e, 0xc3, 0xa5, 0xf9, 0x68, 0xdd,
    0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e, 0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18,
    0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9, 0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e,
    0x5f, 0x08, 0x33, 0x40, 0x76, 0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05,
    0x44, 0x17, 0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23, 0x81,
    0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96, 0xbc, 0x26, 0xeb, 0x32,
    0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc, 0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50,
    0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2, 0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2,
    0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64, 0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6,
    0x21, 0xe7, 0x2d, 0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb, 0xec, 0xf0, 0x10,
    0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0, 0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7,
    0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8, 0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71,
    0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1, 0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62,
    0xde, 0x36, 0xb2, 0xba
};
// asm("_binary_sec2_verifier_end")

// asm("_binary_service_uuid_start")
static uint8_t service_uuid[] = {
    /* LSB <---------------------------------------
     * ---------------------------------------> MSB */
    0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
    0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
};
// asm("_binary_service_uuid_end")

static EventGroupHandle_t netif_event_group;

// ==== Public Implementations ====

esp_err_t dt_provision_init(EventGroupHandle_t netif_event_group) {
    esp_err_t ret = ESP_FAIL;
    OK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provision_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &bluetooth_handler, NULL));
    OK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &session_handler, NULL));
    OK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL));
    OK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_handler, NULL));

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

    // ---- Provisioning Manager ----

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        .wifi_prov_conn_cfg = {
            .wifi_conn_attempts = WIFI_CONN_ATTEMPTS,
        },
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool is_provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&is_provisioned));
    if (is_provisioned) {
        ESP_LOGI(TAG, "WiFi is already provisioned");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        ESP_LOGI(TAG, "Provisioning WiFi access");

        wifi_prov_scheme_ble_set_service_uuid(service_uuid);
        wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
        wifi_prov_security2_params_t sec2_params = {
            .salt = sec2_salt,
            .salt_len = sizeof(sec2_salt),
            .verifier = sec2_verifier,
            .verifier_len = sizeof(sec2_verifier),
        };

        uint8_t eth_mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

        char service_name[12];
        snprintf(service_name, sizeof(service_name), "PROV_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
        ESP_LOGI(TAG, "Device service name: %s", service_name);

        // Must be called before provisioning starts
        wifi_prov_mgr_endpoint_create("custom-data");

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, &sec2_params, service_name, NULL));

        // Must be called after provisioning starts
        wifi_prov_mgr_endpoint_register("custom-data", provision_data_handler, NULL);
    }

    // After provision?
    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), fail, TAG, "Failed to set WiFi mode to station");
    ESP_GOTO_ON_ERROR(esp_wifi_start(), fail, TAG, "Failed to start WiFi");
    return ESP_OK;

fail:
    esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);

    esp_wifi_destroy_if_driver(s_wifi_driver);
    s_wifi_netif = NULL;

    esp_netif_destroy(s_wifi_netif);
    s_wifi_netif = NULL;

    return ret;
}

// ==== Private Implementations ====

static void wifi_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi starting");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
            esp_wifi_connect();
            break;
        default:
            ESP_LOGI(TAG, "Unhandled WiFi event %d", event_id);
            break;
    }
}

static void bluetooth_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "BLE transport connected");
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "BLE transport disconnected");
            break;
        default:
            ESP_LOGI(TAG, "Unhandled BLE event %d", event_id);
            break;
    }
}

static void session_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Secured session established");
            break;
        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            ESP_LOGE(TAG, "Received invald security parameters for establishing secure session");;
            break;
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGE(TAG, "Received incorrect credentials for establishing secure session");
            break;
        default:
            ESP_LOGI(TAG, "Unhandled security session event %d", event_id);
            break;
    }
}

static void provision_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_PROV_INIT:
            ESP_LOGI(TAG, "Provisioning manager initialized");
            break;`
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_SET_STA_CONFIG:
            ESP_LOGI(TAG, "Receiving credentials");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t* cfg = (wifi_sta_config_t*)event_data;
            ESP_LOGI(TAG, "Received credentials - SSID: %s, Password: %s", cfg->ssid, cfg->password);
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t* reason = (wifi_prov_sta_fail_reason_t*)event_data;
            ESP_LOGE(
                TAG,
                "Provisioning failed! Reason: %s",
                (*reason == WIFI_PROV_STA_AUTH_ERROR)
                ? "WiFi station authentication failed"
                : "WiFi access point not found");
            break;
        }
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning done");
            wifi_prov_mgr_deinit();
            break;
        case WIFI_PROV_DEINIT:
            ESP_LOGI(TAG, "Provisioning manager deinitialized");
            break;
        default:
            ESP_LOGI(TAG, "Unhandled provisioning event %d", event_id);
            break;
    }
}

static void ip_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(((ip_event_got_ip_t*)event_data)->ip_info.ip));
            break;
        default:
            ESP_LOGI(TAG, "Unhandled IP event %d", event_id);
            break;
    }
}

static esp_err_t provision_data_handler(
    uint32_t session_id,
    const uint8_t* in_buf,
    ssize_t in_len,
    uint8_t** out_buf,
    ssize_t* out_len,
    void* data) {
    if (in_buf) {
        ESP_LOGI(TAG, "Custom data received: %.*s", in_len, (char*)in_buf);
        char response[] = "SUCCESS";
        *out_buf = (uint8_t*)strdup(response);
        if (*out_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response");
            return ESP_ERR_NO_MEM;
        }
        *out_len = (ssize_t)strlen(response) + 1;
    }

    return ESP_OK;
}
