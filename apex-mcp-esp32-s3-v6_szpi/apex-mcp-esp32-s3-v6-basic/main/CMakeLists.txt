idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES apex_core async_add sync_add bsp_board attitude_app sd_file_app led_app
                    )
