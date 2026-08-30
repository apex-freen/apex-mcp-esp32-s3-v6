idf_component_register(
    SRCS "apex_webserver.c" "apex_webserver_utils.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    REQUIRES esp_http_server                   # 头文件暴露 httpd_handle_t，需公开
    PRIV_REQUIRES apex_config esp_netif apex_network esp_timer apex_event  utils
    # wifi_manager  mqtt_manager utils_manager                    # 声明依赖 NVS 驱动
)
