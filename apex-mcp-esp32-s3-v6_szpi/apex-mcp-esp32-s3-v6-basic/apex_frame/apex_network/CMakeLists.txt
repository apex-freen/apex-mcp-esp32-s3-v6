idf_component_register(
    SRCS "apex_network.c" "apex_network.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    PRIV_INCLUDE_DIRS 
    REQUIRES esp_wifi                    # 声明依赖 NVS 驱动
    PRIV_REQUIRES  esp_netif  freertos apex_config apex_event                     # 声明依赖 NVS 驱动  esp_sntp
)
