cmake_minimum_required(VERSION 3.5)
project(upsd_plugin C)

set(ENABLE_PLUGIN_GO OFF CACHE BOOL "Disable Go plugin")
set(ENABLE_PLUGIN_PYTHON OFF CACHE BOOL "Disable Python plugin")
set(ENABLE_PLUGIN_CHARTS OFF CACHE BOOL "Disable Charts plugin")
set(ENABLE_BUNDLED_PROTOBUF ON CACHE BOOL "Enable bundled protobuf")
set(ENABLE_PLUGIN_SYSTEMD_JOURNAL OFF CACHE BOOL "Disable Systemd Journal plugin")
set(ENABLE_PLUGIN_SYSTEMD_UNITS OFF CACHE BOOL "Disable Systemd Units plugin")
set(ENABLE_PLUGIN_CUPS OFF CACHE BOOL "Disable CUPS plugin")
set(ENABLE_PLUGIN_DEBUGFS OFF CACHE BOOL "Disable DebugFS plugin")
set(ENABLE_PLUGIN_PERF OFF CACHE BOOL "Disable Perf plugin")
set(ENABLE_PLUGIN_SLABINFO OFF CACHE BOOL "Disable Slabinfo plugin")
set(ENABLE_PLUGIN_CGROUP_NETWORK OFF CACHE BOOL "Disable Cgroup Network plugin")
set(ENABLE_PLUGIN_LOCAL_LISTENERS OFF CACHE BOOL "Disable Local Listeners plugin")
set(ENABLE_PLUGIN_NETWORK_VIEWER OFF CACHE BOOL "Disable Network Viewer plugin")
set(ENABLE_PLUGIN_EBPF OFF CACHE BOOL "Disable eBPF plugin")
set(ENABLE_BUNDLED_JSONC OFF CACHE BOOL "Disable bundled json-c")
set(ENABLE_DBENGINE OFF CACHE BOOL "Disable DBEngine")
set(ENABLE_H2O OFF CACHE BOOL "Disable H2O")
set(ENABLE_ML OFF CACHE BOOL "Disable ML")
set(ENABLE_PLUGIN_APPS OFF CACHE BOOL "Disable Apps plugin")
set(ENABLE_EXPORTER_PROMETHEUS_REMOTE_WRITE OFF CACHE BOOL "Disable Prometheus Remote Write Exporter")
set(ENABLE_EXPORTER_MONGODB OFF CACHE BOOL "Disable MongoDB Exporter")
set(ENABLE_PLUGIN_FREEIPMI OFF CACHE BOOL "Disable FreeIPMI plugin")
set(ENABLE_PLUGIN_NFACCT OFF CACHE BOOL "Disable NFACCT plugin")
set(ENABLE_PLUGIN_XENSTAT OFF CACHE BOOL "Disable Xenstat plugin")
add_subdirectory(netdata-2.6.0)

# Find libupsclient
find_library(UPSCLIENT_LIBRARY NAMES upsclient)
if(NOT UPSCLIENT_LIBRARY)
    message(FATAL_ERROR "libupsclient not found")
endif()

# Find libm
find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
    message(FATAL_ERROR "libm not found")
endif()

add_executable(upsd.plugin upsd_plugin.c)
target_link_libraries(upsd.plugin PRIVATE
    libnetdata
    ${UPSCLIENT_LIBRARY}
    ${MATH_LIBRARY}
)
target_include_directories(upsd.plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/netdata
    ${CMAKE_BINARY_DIR}/netdata # For generated files like config.h
)
