idf_component_register(
    SRCS "key_app.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cjson apex_cmd_executor bsp_board
)
