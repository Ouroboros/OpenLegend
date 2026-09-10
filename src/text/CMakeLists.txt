add_library(openlegend_text STATIC
    big5.cpp
    game_text.cpp
)
target_include_directories(
    openlegend_text
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_text
    PUBLIC openlegend_project_options openlegend_project_warnings
)
