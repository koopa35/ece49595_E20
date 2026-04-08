idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES global_defs global_vars hardware_configs rotary_encoder 1602A_OLED eeprom_24xx user_interface dsp ntc driver
                       )