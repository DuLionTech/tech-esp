# Set source files and include directories
set(srcs)
set(include_dirs "include")

# Conditionally add source files
if(CONFIG_DLT_PROVISION)
    list(APPEND srcs "dlt_provision.c")
endif()

# Register the component
idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${include_dirs}"
        PRIV_REQUIRES esp_event)