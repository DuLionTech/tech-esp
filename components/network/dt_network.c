#include "dt_network.h"
#include "dt_provision.h"

esp_err_t dt_network_init(EventGroupHandle_t netif_event_group) {
    OK(dt_provision_init(netif_event_group));
    return ESP_OK;
}
