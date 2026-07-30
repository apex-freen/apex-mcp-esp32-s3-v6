idf_component_register(
    SRCS "apex_ota_update.c"
    INCLUDE_DIRS "include"
    REQUIRES
    PRIV_REQUIRES   freertos apex_config app_update esp_https_ota apex_cmd_executor
)
