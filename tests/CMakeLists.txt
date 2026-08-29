add_executable(
    openlegend_core_tests
    unit/app/mode_coordinator_test.cpp
    unit/compat/legacy_video_test.cpp
)
target_include_directories(openlegend_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_core_tests PRIVATE openlegend_app openlegend_compat)
add_test(NAME openlegend.core COMMAND openlegend_core_tests)

add_executable(openlegend_resource_tests unit/resource/resource_archive_test.cpp)
target_compile_definitions(
    openlegend_resource_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_resource_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_resource_tests PRIVATE openlegend_resource)
add_test(NAME openlegend.resource COMMAND openlegend_resource_tests)

add_executable(openlegend_render_tests unit/render/legacy_render_test.cpp)
target_compile_definitions(
    openlegend_render_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_render_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_render_tests PRIVATE openlegend_render)
add_test(NAME openlegend.render COMMAND openlegend_render_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME openlegend.build_tool
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_tool_test.py
)

if(TARGET openlegend)
    add_test(NAME openlegend.sdl_smoke COMMAND openlegend --smoke-test)
    set_tests_properties(
        openlegend.sdl_smoke
        PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
    )
endif()
