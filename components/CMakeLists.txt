set(include_dirs "")
set(srcs "")
set(priv_requires "")

if (CONFIG_DLT_NETWORK_SUPPORT)
    list(APPEND include_dirs "network/include")
    list(APPEND srcs "network/dlt_network.c")
endif ()

if (CONFIG_DLT_PROVISION)
    list(APPEND include_dirs "provision/include")
    list(APPEND srcs "provision/dlt_provision.c")
endif ()

idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${include_dirs}"
        PRIV_REQUIRES "${priv_requires}")
