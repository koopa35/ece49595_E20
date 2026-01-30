idf_component_register(SRCS "main.c" "i2s.c" "1602A_OLED.c" "filters.c" PRIV_REQUIRES esp_driver_spi esp_driver_i2s esp_driver_gptimer esp_driver_gpio INCLUDE_DIRS ".")
