idf_component_register(SRCS "menu.c"
                    REQUIRES driver freertos rotary_encoder
                    PRIV_REQUIRES 1602A_OLED
                    INCLUDE_DIRS "include")
