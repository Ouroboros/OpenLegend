add_library(openlegend_app STATIC mode_coordinator.cpp)
target_include_directories(
    openlegend_app
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_app
    PUBLIC openlegend_compat openlegend_project_options openlegend_project_warnings
)
