idf_component_register(
    SRCS "bsp_board.c" "bsp_i2c.c" "bsp_imu.c" "bsp_sd.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c
    PRIV_REQUIRES freertos log sdmmc fatfs esp_driver_sdmmc
)
