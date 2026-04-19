#include "dlt_network.h"
#include "dlt_wifi.h"

esp_err_t dt_network_start(EventGroupHandle_t netif_event_group) {
    OK(esp_netif_init());
    OK(dlt_wifi_start(netif_event_group));
    return ESP_OK;
}
