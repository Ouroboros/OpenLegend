add_library(openlegend_diagnostics STATIC log.cpp)
target_include_directories(
    openlegend_diagnostics
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_diagnostics
    PUBLIC openlegend_project_options openlegend_project_warnings
)
