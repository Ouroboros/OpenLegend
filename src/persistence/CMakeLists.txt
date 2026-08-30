add_library(openlegend_persistence STATIC save_slot.cpp)
target_include_directories(
    openlegend_persistence
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_persistence
    PUBLIC
        openlegend_compat
        openlegend_model
        openlegend_resource
        openlegend_project_options
        openlegend_project_warnings
)
