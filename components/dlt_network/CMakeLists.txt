set(srcs "")
set(include_dirs "")

if (CONFIG_DLT_NETWORK_SUPPORT)
    list(APPEND include_dirs "include")
    list(APPEND srcs "dlt_network.c")
endif ()

idf_component_register(
        SRCS ${srcs}
        INCLUDE_DIRS ${include_dirs}
        PRIV_REQUIRES dlt dlt_wifi esp_netif)
