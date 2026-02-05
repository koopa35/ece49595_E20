idf_component_register(
    SRCS "runtime_log.c"
    INCLUDE_DIRS "include"
    REQUIRES eeprom_24xx esp_timer
)
