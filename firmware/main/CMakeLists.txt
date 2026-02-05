idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES i2c_bus eeprom_24xx runtime_log
)
