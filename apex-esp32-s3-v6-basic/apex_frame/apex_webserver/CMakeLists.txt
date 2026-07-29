idf_component_register(
    SRCS "apex_webserver.c" "apex_webserver_utils.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    PRIV_REQUIRES esp_http_server apex_config esp_netif apex_network esp_timer apex_event  utils
    # wifi_manager  mqtt_manager utils_manager                    # 声明依赖 NVS 驱动
)
