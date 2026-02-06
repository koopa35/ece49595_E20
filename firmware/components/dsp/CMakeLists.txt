idf_component_register(SRCS "dsp.c"
                    REQUIRES global_defs global_vars hardware_configs 1602A_OLED user_interface driver esp-dsp
                    INCLUDE_DIRS "include")
