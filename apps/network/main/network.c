#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <dlt_network.h>
#include "../../../components/provision/include/sec2.h"

#define OK ESP_ERROR_CHECK

static const char* TAG = "network";

static EventGroupHandle_t wifi_event_group;

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        OK(nvs_flash_erase());
        OK(nvs_flash_init());
    }

    OK(esp_netif_init());
    OK(esp_event_loop_create_default());

    wifi_event_group = xEventGroupCreate();

    // esp_netif_create_default_wifi_sta();
}
