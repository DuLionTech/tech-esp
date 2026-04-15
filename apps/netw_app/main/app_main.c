#include <string.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <dt_err.h>
#include <dt_network.h>

#include <freertos/task.h>

static const char* TAG = "network";

static EventGroupHandle_t netif_event_group;

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        OK(nvs_flash_erase());
        OK(nvs_flash_init());
    }

    OK(esp_netif_init());
    OK(esp_event_loop_create_default());
    netif_event_group = xEventGroupCreate();
    OK(dt_network_start(netif_event_group));
    xEventGroupWaitBits(netif_event_group, NETIF_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // esp_netif_create_default_wifi_sta();
}
