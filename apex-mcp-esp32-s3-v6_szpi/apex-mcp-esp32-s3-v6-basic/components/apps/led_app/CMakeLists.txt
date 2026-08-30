idf_component_register(
    SRCS "led_app.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cjson apex_cmd_executor freertos esp_driver_gpio
)
