idf_component_register(SRCS "apex_crypto.c"
                    INCLUDE_DIRS "include"
                    # PRIV_REQUIRES nvs_flash apex_config esp_event
                    # esp_wifi esp_netif freertos  esp_http_server esp_timer  apex_event apex_wifi_mgr   
                    PRIV_REQUIRES mbedtls esp_timer
                    )
