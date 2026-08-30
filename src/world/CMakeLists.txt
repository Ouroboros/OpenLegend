add_library(openlegend_world STATIC world_map.cpp)
target_include_directories(
    openlegend_world
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(openlegend_world PRIVATE openlegend_diagnostics)
target_link_libraries(
    openlegend_world
    PUBLIC
        openlegend_model
        openlegend_random
        openlegend_render
        openlegend_resource
        openlegend_project_options
        openlegend_project_warnings
)
