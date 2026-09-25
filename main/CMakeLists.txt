idf_component_register(SRCS "main.c" "ble_link.c" "node_scan.c" "rules.c" "mesh_protocol.c" "bbs.c" "settings.c" "provision.c" "setup_protocol.c" "status_led.c" "activity.c" "diagnostics.c" "bulletins.c" "owner.c" "bans.c"
    INCLUDE_DIRS "." REQUIRES cjson mbedtls bt nvs_flash esp_timer esp_driver_uart esp_driver_usb_serial_jtag esp_driver_gpio esp_driver_rmt meshtastic)
