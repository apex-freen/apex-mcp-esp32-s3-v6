idf_component_register(
    SRCS "bsp_board.c" "bsp_i2c.c" "bsp_imu.c" "bsp_sd.c" "bsp_pca9557.c"
         "bsp_lcd.c" "bsp_touch.c" "bsp_key.c" "bsp_font.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi
    PRIV_REQUIRES freertos log heap esp_lcd esp_driver_ledc
                 esp_lcd_touch_ft5x06 sdmmc fatfs esp_driver_sdmmc
)
