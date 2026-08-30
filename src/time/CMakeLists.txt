add_library(openlegend_time legacy_clock.cpp)
target_include_directories(openlegend_time PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    openlegend_time
    PUBLIC openlegend_project_options
    PRIVATE openlegend_project_warnings
)
