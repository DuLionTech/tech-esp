#include <string.h>
#include <dlt_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_netif.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <esp_private/wifi.h>

#include <sys/types.h>

#include <esp_check.h>
#include <lwip/ip4_addr.h>
#include <lwip/priv/nd6_priv.h>
#include <dlt_event.h>

#include "dlt_wifi.h"
#include "dlt_sec2.h"

#define WIFI_CONN_ATTEMPTS 5

static const char* TAG = "dlt_wifi";
static esp_netif_t* s_wifi_netif = NULL;
static wifi_netif_driver_t s_wifi_driver = NULL;
static EventGroupHandle_t s_netif_event_group;

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
static void wifi_start(esp_event_base_t event_base, int32_t event_id, void* event_data);

// ==== Public Implementations ====

esp_err_t dlt_wifi_start(EventGroupHandle_t netif_event_group) {
    esp_err_t ret = ESP_FAIL;
    s_netif_event_group = netif_event_group;
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

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_GOTO_ON_ERROR(esp_wifi_init(&wifi_config), fail, TAG, "Failed to initialize WiFi");

    // ---- Provisioning Manager ----

    wifi_prov_mgr_config_t prov_config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        .wifi_prov_conn_cfg = {
            .wifi_conn_attempts = WIFI_CONN_ATTEMPTS,
        },
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

    bool is_provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&is_provisioned));
    if (is_provisioned) {
        ESP_LOGI(TAG, "WiFi is already provisioned");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "WiFi started");
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
            wifi_start(event_base, event_id, event_data);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected");
            // Section 2.4 Register interface receive callback
            wifi_netif_driver_t driver = esp_netif_get_io_driver(s_wifi_netif);
            if (!esp_wifi_is_if_ready_when_started(driver)) {
                esp_err_t esp_ret = esp_wifi_register_if_rxcb(driver, esp_netif_receive, s_wifi_netif);
                if (esp_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register interface receive callback");
                    break;
                }
            }

            // Section 4.2 Set up the WiFi interface and start DHCP process
            esp_netif_action_connected(s_wifi_netif, event_base, event_id, event_data);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_HOME_CHANNEL_CHANGE: {
            wifi_event_home_channel_change_t* event = (wifi_event_home_channel_change_t*)event_data;
            ESP_LOGI(
                TAG,
                "WiFi channel changed from %d/%d to %d/%d",
                event->old_chan,
                event->old_snd,
                event->new_chan,
                event->new_snd);
            break;
        }
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
            break;
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
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(s_netif_event_group, DLT_NET_CONNECTED_BIT);
            break;
        }
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "Lost IP");
            xEventGroupClearBits(s_netif_event_group, DLT_NET_CONNECTED_BIT);
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

static void wifi_start(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    uint8_t mac_addr[6] = {0};
    esp_err_t esp_ret = ESP_OK;

    wifi_netif_driver_t driver = esp_netif_get_io_driver(s_wifi_netif);
    if (driver == NULL) {
        ESP_LOGE(TAG, "Failed to get WiFi driver");
        return;
    }

    esp_ret = esp_wifi_get_if_mac(driver, mac_addr);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi MAC address: %d", esp_ret);
    }
    ESP_LOGI(
        TAG,
        "WiFi MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
        mac_addr[0],
        mac_addr[1],
        mac_addr[2],
        mac_addr[3],
        mac_addr[4],
        mac_addr[5]);

    esp_ret = esp_wifi_internal_reg_netstack_buf_cb(esp_netif_netstack_buf_ref, esp_netif_netstack_buf_free);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi netstack buffer callbacks: %d", esp_ret);
    }

    // Section 1.3 Set MAC address of the WiFi interface
    esp_ret = esp_netif_set_mac(s_wifi_netif, mac_addr);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi MAC address: %d", esp_ret);
    }
    esp_netif_action_start(s_wifi_netif, event_base, event_id, event_data);

    // Section 3.3 Connect to WiFi
    esp_ret = esp_wifi_connect();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %d", esp_ret);
    }
}
