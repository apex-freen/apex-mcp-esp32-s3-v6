idf_component_register(
    SRCS "apex_notify.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES freertos apex_config apex_cmd_executor
)
