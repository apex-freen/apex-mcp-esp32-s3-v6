idf_component_register(
    SRCS "apex_reset.c" "apex_reset.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    PRIV_REQUIRES esp_event  nvs_flash  apex_cmd_executor  cjson  apex_config             # 声明依赖 NVS 驱动
)
