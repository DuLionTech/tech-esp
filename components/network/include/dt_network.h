#ifndef DT_NETWORK_H
#define DT_NETWORK_H
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

esp_err_t dt_network_init(EventGroupHandle_t netif_event_group);

#endif // DT_NETWORK_H