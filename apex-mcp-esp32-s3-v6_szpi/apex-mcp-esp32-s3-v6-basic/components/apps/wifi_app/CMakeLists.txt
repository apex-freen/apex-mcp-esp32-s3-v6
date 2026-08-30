idf_component_register(
    SRCS "wifi_app.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cjson apex_cmd_executor apex_network esp_wifi
)
