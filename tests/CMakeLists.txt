add_executable(
    openlegend_core_tests
    unit/app/mode_coordinator_test.cpp
    unit/app/runtime_configuration_test.cpp
    unit/compat/legacy_video_test.cpp
    unit/input/legacy_runtime_test.cpp
)
target_include_directories(openlegend_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_core_tests
    PRIVATE openlegend_app openlegend_compat openlegend_input openlegend_random openlegend_time
)
add_test(NAME openlegend.core COMMAND openlegend_core_tests)

add_executable(openlegend_model_tests unit/model/new_game_test.cpp)
target_include_directories(openlegend_model_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_model_tests PRIVATE openlegend_model)
add_test(NAME openlegend.model COMMAND openlegend_model_tests)

add_executable(openlegend_resource_tests unit/resource/resource_archive_test.cpp)
target_compile_definitions(
    openlegend_resource_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_resource_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_resource_tests PRIVATE openlegend_resource)
add_test(NAME openlegend.resource COMMAND openlegend_resource_tests)

add_executable(openlegend_ui_tests unit/ui/title_menu_test.cpp)
target_compile_definitions(
    openlegend_ui_tests
    PRIVATE
        OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
        OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_ui_tests PRIVATE openlegend_app openlegend_ui)
add_test(NAME openlegend.ui COMMAND openlegend_ui_tests)

add_executable(openlegend_audio_tests unit/audio/legacy_audio_test.cpp)
target_compile_definitions(
    openlegend_audio_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_audio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_audio_tests PRIVATE openlegend_audio openlegend_resource)
add_test(NAME openlegend.audio COMMAND openlegend_audio_tests)

add_executable(openlegend_render_tests unit/render/legacy_render_test.cpp)
target_compile_definitions(
    openlegend_render_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_render_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_render_tests PRIVATE openlegend_render)
add_test(NAME openlegend.render COMMAND openlegend_render_tests)

add_executable(openlegend_world_tests unit/world/world_map_test.cpp)
target_compile_definitions(
    openlegend_world_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_world_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_world_tests
    PRIVATE openlegend_persistence openlegend_resource openlegend_world
)
add_test(NAME openlegend.world COMMAND openlegend_world_tests)

add_executable(openlegend_scene_tests unit/scene/scene_test.cpp)
target_compile_definitions(
    openlegend_scene_tests
    PRIVATE OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
)
target_include_directories(openlegend_scene_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_scene_tests
    PRIVATE openlegend_persistence openlegend_resource openlegend_scene
)
add_test(NAME openlegend.scene COMMAND openlegend_scene_tests)

add_executable(openlegend_persistence_tests unit/persistence/save_slot_test.cpp)
target_compile_definitions(
    openlegend_persistence_tests
    PRIVATE
        OPENLEGEND_GAME_DATA_ROOT="${PROJECT_SOURCE_DIR}/.."
        OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_persistence_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_persistence_tests
    PRIVATE openlegend_model openlegend_persistence openlegend_resource
)
add_test(NAME openlegend.persistence COMMAND openlegend_persistence_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME openlegend.build_tool
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_tool_test.py
)

if(TARGET openlegend)
    add_test(
        NAME openlegend.sdl_smoke
        COMMAND openlegend --smoke-test --data-dir=${PROJECT_SOURCE_DIR}/..
    )
    set_tests_properties(
        openlegend.sdl_smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy"
            TIMEOUT 30
    )
endif()
