add_executable(
    openlegend
    frame_presenter.cpp
    legacy_input_coordinator.cpp
    legacy_runtime_loop.cpp
    main.cpp
    sdl_application.cpp
    sdl_audio_device.cpp
    sdl_runtime_platform.cpp
)
target_include_directories(openlegend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    openlegend
    PRIVATE
        openlegend_app
        openlegend_audio
        openlegend_compat
        openlegend_diagnostics
        openlegend_input
        openlegend_project_options
        openlegend_project_warnings
        openlegend_time
        SDL3::SDL3
)
if(WIN32)
    target_link_libraries(openlegend PRIVATE shell32)
endif()
