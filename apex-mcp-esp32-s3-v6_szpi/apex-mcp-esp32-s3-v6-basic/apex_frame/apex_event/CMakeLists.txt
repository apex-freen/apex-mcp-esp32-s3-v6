idf_component_register(
    SRCS "apex_event.c" "apex_event.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    # PRIV_REQUIRES esp_event                     # 声明依赖 NVS 驱动
    REQUIRES esp_event                     # 声明依赖 NVS 驱动
)
