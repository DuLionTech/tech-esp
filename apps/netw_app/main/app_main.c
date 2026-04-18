#include <string.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <freertos/task.h>

#include <dlt_err.h>
#include <dlt_event.h>
#include <dlt_network.h>

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

    ESP_LOGI(TAG, "Waiting for network connection");
    xEventGroupWaitBits(netif_event_group, DLT_NET_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        ESP_LOGI(TAG, "Running!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

