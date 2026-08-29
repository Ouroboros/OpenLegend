add_executable(openlegend main.cpp sdl_runtime_platform.cpp)
target_include_directories(openlegend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    openlegend
    PRIVATE
        openlegend_app
        openlegend_compat
        openlegend_project_options
        openlegend_project_warnings
        SDL3::SDL3
)
