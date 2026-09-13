add_library(openlegend_motion STATIC authoritative_motion.cpp)
target_include_directories(openlegend_motion PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    openlegend_motion
    PUBLIC openlegend_project_options
    PRIVATE openlegend_project_warnings
)
