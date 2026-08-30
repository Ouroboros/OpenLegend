add_library(openlegend_scene STATIC scene.cpp)
target_include_directories(
    openlegend_scene
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(openlegend_scene PRIVATE openlegend_diagnostics)
target_link_libraries(
    openlegend_scene
    PUBLIC
        openlegend_model
        openlegend_random
        openlegend_render
        openlegend_resource
        openlegend_project_options
        openlegend_project_warnings
)
