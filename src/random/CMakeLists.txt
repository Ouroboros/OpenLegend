add_library(openlegend_random legacy_random.cpp)
target_include_directories(openlegend_random PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    openlegend_random
    PUBLIC openlegend_project_options
    PRIVATE openlegend_project_warnings
)
