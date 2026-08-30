idf_component_register(
    SRCS "apex_config.c" "apex_config.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    REQUIRES nvs_flash  spi_flash  apex_event                 # 声明依赖 NVS 驱动 cjson
)
