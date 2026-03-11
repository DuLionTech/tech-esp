set(include_dirs "")
set(srcs "")
set(priv_requires "")

if (CONFIG_DLT_NETWORK_SUPPORT)
    list(APPEND include_dirs "network/include")
    list(APPEND srcs "network/dlt_network.c")
    list(APPEND priv_requires)
endif ()

if (CONFIG_DLT_PROVISION)
    list(APPEND srcs "provision/dlt_provision.c")
    list(APPEND include_dirs "provision/include")
    list(APPEND priv_requires esp_netif esp_wifi wifi_provisioning esp_event)
    message("HERE INSIDE!!!! ${priv_requires}")
endif ()
message("HERE OUTSIDE!!!! ${priv_requires}")

idf_component_register(
        SRCS ${srcs}
        PRIV_REQUIRES ${priv_requires}
        INCLUDE_DIRS ${include_dirs})
