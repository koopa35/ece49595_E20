idf_component_register(
    SRCS "ina260.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos global_vars global_defs
)
