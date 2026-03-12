set(srcs "")
set(include_dirs "")

if (CONFIG_DT_PROVISION)
    list(APPEND srcs "dt_provision.c")
    list(APPEND include_dirs "include")
endif ()

idf_component_register(
        SRCS ${srcs}
        INCLUDE_DIRS ${include_dirs}
        PRIV_REQUIRES dulion_tech esp_netif esp_wifi wifi_provisioning bt esp_event)
