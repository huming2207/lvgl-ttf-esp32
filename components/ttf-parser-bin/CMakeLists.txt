idf_component_register(
        SRC
            ttfparser.h
        INCLUDE_DIRS
            .
)

add_prebuilt_library(ttfparser "libttfparser.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE -Wl,--allow-multiple-definition ttfparser -Wl,--allow-multiple-definition)