idf_component_register(SRCS "main.c" "sdm.c" "adc.c" "1602A_OLED.c" PRIV_REQUIRES esp_driver_spi esp_driver_sdm esp_driver_gptimer esp_driver_gpio esp_adc INCLUDE_DIRS ".")
