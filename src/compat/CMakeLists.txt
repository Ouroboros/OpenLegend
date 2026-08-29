add_library(openlegend_compat INTERFACE)
target_include_directories(
    openlegend_compat
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(openlegend_compat INTERFACE openlegend_project_options)
