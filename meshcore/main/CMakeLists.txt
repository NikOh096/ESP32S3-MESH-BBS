set(COMMON "${CMAKE_CURRENT_LIST_DIR}/../../main")
idf_component_register(SRCS "main.c" "ble_link.c" "meshcore_protocol.c" "meshcore_ids.c" "${COMMON}/node_scan.c" "settings.c" "diagnostics.c"
    "${COMMON}/bulletins.c" "${COMMON}/rules.c" "${COMMON}/bans.c" "${COMMON}/owner.c"
    "${COMMON}/provision.c" "${COMMON}/status_led.c" "${COMMON}/activity.c"
    INCLUDE_DIRS "." "${COMMON}"
    REQUIRES cjson mbedtls bt nvs_flash esp_timer esp_driver_uart esp_driver_usb_serial_jtag esp_driver_gpio esp_driver_rmt)
target_compile_definitions(${COMPONENT_LIB} PUBLIC MESHBBS_MESHCORE=1)
get_filename_component(COMMON_REAL "${COMMON}" REALPATH)
# Shared sources should report a project path, never the builder's home directory.
target_compile_options(${COMPONENT_LIB} PRIVATE "-fmacro-prefix-map=${COMMON_REAL}=main")
