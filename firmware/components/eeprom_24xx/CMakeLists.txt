idf_component_register(
    SRCS "eeprom_24xx.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos global_vars global_defs
)
