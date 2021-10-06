idf_component_register(
        SRCS
            "main.cpp"
            "display/hal/lv_hal_st7789.c" "display/hal/includes/lv_hal_st7789.h"
        INCLUDE_DIRS
            "." "display/hal/includes"
        REQUIRES
            lvgl ttfparser bisheng
        EMBED_FILES
            resource/wqy.ttf
)

