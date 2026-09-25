idf_component_register(SRCS
    "meshtastic/mesh.pb.c" "meshtastic/channel.pb.c" "meshtastic/config.pb.c"
    "meshtastic/device_ui.pb.c" "meshtastic/module_config.pb.c"
    "meshtastic/portnums.pb.c" "meshtastic/telemetry.pb.c" "meshtastic/xmodem.pb.c" "meshtastic/atak.pb.c"
    INCLUDE_DIRS "." REQUIRES nanopb)
