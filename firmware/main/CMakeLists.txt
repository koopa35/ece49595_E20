idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES global_defs global_vars hardware_configs rotary_encoder 1602A_OLED eeprom user_interface dsp driver
                       )