idf_component_register(
        SRCS
            "main.c"
            "display/hal/lv_hal_st7789.c" "display/hal/includes/lv_hal_st7789.h"
            "display/external/includes/stb_truetype.h"
        INCLUDE_DIRS
            "." "display/hal/includes" "display/external/includes"
        REQUIRES
            lvgl ttfparser
        EMBED_FILES
            resource/wqy.ttf
)

