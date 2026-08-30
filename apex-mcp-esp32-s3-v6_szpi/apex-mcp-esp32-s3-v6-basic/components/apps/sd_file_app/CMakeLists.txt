idf_component_register(
    SRCS "sd_file_app.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cjson apex_cmd_executor bsp_board freertos esp_libc
)
