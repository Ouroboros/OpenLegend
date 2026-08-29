add_library(
    openlegend_resource
    STATIC
        binary_file.cpp
        legacy_assets.cpp
        legacy_sprite.cpp
        packed_archive.cpp
)
target_include_directories(
    openlegend_resource
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_resource
    PUBLIC openlegend_compat openlegend_project_options openlegend_project_warnings
)
