idf_component_register(
    SRCS "apex_power_up.c" "apex_power_up.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    PRIV_REQUIRES esp_event    apex_cmd_executor  cjson               # 声明依赖 NVS 驱动
)
