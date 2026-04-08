idf_component_register(SRCS "hardware_configs.c"
                    REQUIRES global_defs rotary_encoder eeprom_24xx driver esp_adc
                    INCLUDE_DIRS "include")
