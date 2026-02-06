idf_component_register(SRCS "user_interface.c"
                    REQUIRES global_defs global_vars rotary_encoder 1602A_OLED driver
                    INCLUDE_DIRS "include")
