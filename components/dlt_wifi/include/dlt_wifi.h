#ifndef DLT_PROVISION_H
#define DLT_PROVISION_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <dlt_error.h>

esp_err_t dlt_wifi_start(EventGroupHandle_t netif_event_group);

#endif // DLT_PROVISION_H