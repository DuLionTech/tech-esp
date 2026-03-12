#ifndef DT_PROVISION_H
#define DT_PROVISION_H
#include <dt_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

esp_err_t dt_provision_init(EventGroupHandle_t netif_event_group);

#endif // DT_PROVISION_H