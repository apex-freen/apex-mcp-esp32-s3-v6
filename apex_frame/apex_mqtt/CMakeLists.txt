idf_component_register(
    SRCS "apex_mqtt.c" "apex_mqtt.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    PRIV_INCLUDE_DIRS 
        # "src"                   # 内部头文件目录，仅组件内可见
    # REQUIRES esp_wifi                    # 声明依赖 NVS 驱动
    # PRIV_REQUIRES  esp_netif  freertos apex_config apex_event                     # 声明依赖 NVS 驱动
    REQUIRES  apex_config apex_event freertos mqtt cjson  apex_cmd_executor esp_http_client
)
