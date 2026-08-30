add_library(openlegend_input legacy_keyboard.cpp)
target_include_directories(openlegend_input PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    openlegend_input
    PUBLIC openlegend_compat openlegend_project_options
    PRIVATE openlegend_project_warnings
)
