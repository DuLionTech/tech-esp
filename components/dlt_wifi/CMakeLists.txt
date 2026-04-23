set(srcs "")
set(include_dirs "")

if (CONFIG_DLT_WIFI)
    list(APPEND srcs "dlt_wifi.c")
    list(APPEND include_dirs "include")
endif ()

idf_component_register(
        SRCS ${srcs}
        INCLUDE_DIRS ${include_dirs}
        PRIV_REQUIRES dlt esp_netif esp_wifi wifi_provisioning bt esp_event nvs_flash esp_partition)
