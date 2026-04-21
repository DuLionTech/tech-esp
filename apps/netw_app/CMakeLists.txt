cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
list(APPEND EXTRA_COMPONENT_DIRS "../../components")
project(netw_app)

target_add_binary_data(netw_app.elf "extern/salt.bin" BINARY)
target_add_binary_data(netw_app.elf "extern/verifier.bin" BINARY)
target_add_binary_data(netw_app.elf "extern/uuid.bin" BINARY)
