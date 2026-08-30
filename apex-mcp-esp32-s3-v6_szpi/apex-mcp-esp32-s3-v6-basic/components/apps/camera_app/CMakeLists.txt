idf_component_register(
    SRCS "camera_app.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cjson apex_cmd_executor bsp_board apex_webserver esp_netif esp_libc heap esp32-camera
)
