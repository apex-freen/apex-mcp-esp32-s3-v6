idf_component_register(
    SRCS "apex_cmd_executor.c" "apex_cmd_executor.c"
    INCLUDE_DIRS "include"                     # 这里的 include 是 apex_config 自己的头文件目录
    REQUIRES cjson 
    PRIV_REQUIRES apex_config apex_mqtt apex_crypto mbedtls esp_http_client apex_get_state apex_restart apex_reset apex_power_up apex_power_down apex_stop apex_ota_update

)
