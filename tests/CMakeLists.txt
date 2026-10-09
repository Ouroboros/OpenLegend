find_package(Threads REQUIRED)

function(add_openlegend_test_shards TEST_PREFIX TEST_TARGET SHARD_COUNT)
    math(EXPR LAST_TEST_SHARD "${SHARD_COUNT} - 1")
    foreach(TEST_SHARD_INDEX RANGE 0 ${LAST_TEST_SHARD})
        math(EXPR TEST_SHARD_NUMBER "${TEST_SHARD_INDEX} + 1")
        set(TEST_NAME "${TEST_PREFIX}.${TEST_SHARD_NUMBER}-of-${SHARD_COUNT}")
        set(
            TEST_TEMP_ROOT
            "${CMAKE_CURRENT_BINARY_DIR}/tmp/${TEST_TARGET}/${TEST_SHARD_NUMBER}-of-${SHARD_COUNT}"
        )
        file(MAKE_DIRECTORY "${TEST_TEMP_ROOT}")
        add_test(
            NAME "${TEST_NAME}"
            COMMAND ${TEST_TARGET} "${TEST_SHARD_INDEX}" "${SHARD_COUNT}"
        )
        set_tests_properties(
            "${TEST_NAME}"
            PROPERTIES ENVIRONMENT
                "OPENLEGEND_GAME_DATA_ROOT=${OPENLEGEND_GAME_DATA_ROOT};TMP=${TEST_TEMP_ROOT};TEMP=${TEST_TEMP_ROOT};TMPDIR=${TEST_TEMP_ROOT}"
        )
    endforeach()
endfunction()

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

add_executable(openlegend_checked_arithmetic_tests unit/model/checked_arithmetic_test.cpp)
target_include_directories(openlegend_checked_arithmetic_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_checked_arithmetic_tests PRIVATE openlegend_model)
add_test(NAME openlegend.checked_arithmetic COMMAND openlegend_checked_arithmetic_tests)

add_executable(openlegend_experience_tests unit/model/experience_test.cpp)
target_include_directories(openlegend_experience_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_experience_tests PRIVATE openlegend_battle)
add_test(NAME openlegend.experience COMMAND openlegend_experience_tests)

add_executable(openlegend_enemy_growth_tests unit/model/enemy_growth_test.cpp)
target_include_directories(openlegend_enemy_growth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_enemy_growth_tests PRIVATE openlegend_model)
add_test(NAME openlegend.enemy_growth COMMAND openlegend_enemy_growth_tests)

add_executable(openlegend_magic_progression_tests unit/model/magic_progression_test.cpp)
target_include_directories(openlegend_magic_progression_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_magic_progression_tests PRIVATE openlegend_model openlegend_persistence)
add_test(NAME openlegend.magic_progression COMMAND openlegend_magic_progression_tests)

add_executable(openlegend_hurt_tests unit/model/hurt_test.cpp)
target_include_directories(openlegend_hurt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_hurt_tests PRIVATE openlegend_model)
add_test(NAME openlegend.hurt COMMAND openlegend_hurt_tests)

add_executable(openlegend_medicine_tests unit/model/medicine_test.cpp)
target_include_directories(openlegend_medicine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_medicine_tests PRIVATE openlegend_model)
add_test(NAME openlegend.medicine COMMAND openlegend_medicine_tests)

add_executable(openlegend_poison_tests unit/model/poison_test.cpp)
target_include_directories(openlegend_poison_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_poison_tests PRIVATE openlegend_model)
add_test(NAME openlegend.poison COMMAND openlegend_poison_tests)

add_executable(openlegend_practice_tests unit/model/practice_test.cpp)
target_include_directories(openlegend_practice_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_practice_tests PRIVATE openlegend_battle)
add_test(NAME openlegend.practice COMMAND openlegend_practice_tests)

add_executable(openlegend_role_state_tests unit/model/role_state_test.cpp)
target_include_directories(openlegend_role_state_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_role_state_tests PRIVATE openlegend_model openlegend_persistence)
add_test(NAME openlegend.role_state COMMAND openlegend_role_state_tests)

add_executable(openlegend_runtime_snapshot_tests unit/model/runtime_snapshot_test.cpp)
target_include_directories(openlegend_runtime_snapshot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_runtime_snapshot_tests PRIVATE openlegend_model openlegend_persistence)
add_test(NAME openlegend.runtime_snapshot COMMAND openlegend_runtime_snapshot_tests)

add_executable(openlegend_ngplus_configuration_tests unit/app/new_game_plus_configuration_test.cpp)
target_include_directories(openlegend_ngplus_configuration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_ngplus_configuration_tests PRIVATE openlegend_app)
add_test(NAME openlegend.ngplus_configuration COMMAND openlegend_ngplus_configuration_tests)

set_tests_properties(
    openlegend.role_state openlegend.runtime_snapshot openlegend.experience openlegend.practice openlegend.magic_progression PROPERTIES
    ENVIRONMENT "OPENLEGEND_GAME_DATA_ROOT=${OPENLEGEND_GAME_DATA_ROOT}"
)

add_executable(
    openlegend_motion_tests
    unit/motion/authoritative_motion_test.cpp
)
target_include_directories(openlegend_motion_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_motion_tests PRIVATE openlegend_motion)
add_test(NAME openlegend.motion COMMAND openlegend_motion_tests)

add_executable(openlegend_resource_tests unit/resource/resource_archive_test.cpp)
target_include_directories(openlegend_resource_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_resource_tests PRIVATE openlegend_resource)
add_test(NAME openlegend.resource COMMAND openlegend_resource_tests)

add_executable(openlegend_ui_tests unit/ui/title_menu_test.cpp)
target_compile_definitions(
    openlegend_ui_tests
    PRIVATE OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_ui_tests PRIVATE openlegend_app openlegend_ui)
add_openlegend_test_shards(openlegend.ui openlegend_ui_tests 18)

add_executable(openlegend_audio_tests unit/audio/legacy_audio_test.cpp)
target_include_directories(openlegend_audio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_audio_tests PRIVATE openlegend_audio openlegend_resource)
add_test(NAME openlegend.audio COMMAND openlegend_audio_tests)

add_executable(openlegend_render_tests unit/render/legacy_render_test.cpp)
target_include_directories(openlegend_render_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_render_tests PRIVATE openlegend_render)
add_test(NAME openlegend.render COMMAND openlegend_render_tests)

add_executable(
    openlegend_indexed_sprite_tests
    unit/render/indexed_sprite_image_test.cpp
)
target_include_directories(
    openlegend_indexed_sprite_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support
)
target_link_libraries(openlegend_indexed_sprite_tests PRIVATE openlegend_render)
add_test(NAME openlegend.indexed_sprite COMMAND openlegend_indexed_sprite_tests)

add_executable(
    openlegend_indexed_layer_tests
    unit/render/indexed_layer_test.cpp
)
target_include_directories(
    openlegend_indexed_layer_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support
)
target_link_libraries(openlegend_indexed_layer_tests PRIVATE openlegend_render)
add_test(NAME openlegend.indexed_layer COMMAND openlegend_indexed_layer_tests)

add_executable(
    openlegend_native_motion_projection_tests
    unit/render/native_motion_projection_test.cpp
)
target_include_directories(
    openlegend_native_motion_projection_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support
)
target_link_libraries(
    openlegend_native_motion_projection_tests
    PRIVATE openlegend_render
)
add_test(
    NAME openlegend.native_motion_projection
    COMMAND openlegend_native_motion_projection_tests
)

add_executable(openlegend_world_tests unit/world/world_map_test.cpp)
target_include_directories(openlegend_world_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_world_tests
    PRIVATE openlegend_persistence openlegend_resource openlegend_world
)
add_test(NAME openlegend.world COMMAND openlegend_world_tests)

add_executable(openlegend_scene_tests unit/scene/scene_test.cpp)
target_include_directories(openlegend_scene_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_scene_tests
    PRIVATE openlegend_persistence openlegend_resource openlegend_scene
)
add_openlegend_test_shards(openlegend.scene openlegend_scene_tests 47)

add_executable(openlegend_battle_tests unit/battle/battle_data_test.cpp)
target_compile_definitions(
    openlegend_battle_tests
    PRIVATE OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_battle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_battle_tests PRIVATE openlegend_battle)
add_openlegend_test_shards(openlegend.battle openlegend_battle_tests 53)

add_executable(openlegend_battle_enemy_roles_tests unit/battle/enemy_roles_test.cpp)
target_include_directories(openlegend_battle_enemy_roles_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(openlegend_battle_enemy_roles_tests PRIVATE openlegend_battle openlegend_persistence)
add_test(NAME openlegend.battle.enemy_roles COMMAND openlegend_battle_enemy_roles_tests)

add_executable(openlegend_persistence_tests unit/persistence/save_slot_test.cpp)
target_compile_definitions(
    openlegend_persistence_tests
    PRIVATE OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_persistence_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_persistence_tests
    PRIVATE openlegend_model openlegend_persistence openlegend_resource
)
add_test(NAME openlegend.persistence COMMAND openlegend_persistence_tests)

set_tests_properties(
    openlegend.resource
    openlegend.audio
    openlegend.render
    openlegend.world
    openlegend.persistence
    openlegend.battle.enemy_roles
    PROPERTIES ENVIRONMENT "OPENLEGEND_GAME_DATA_ROOT=${OPENLEGEND_GAME_DATA_ROOT}"
)

add_executable(openlegend_diagnostics_tests unit/diagnostics/log_test.cpp)
target_compile_definitions(
    openlegend_diagnostics_tests
    PRIVATE OPENLEGEND_TEST_OUTPUT_ROOT="${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>"
)
target_include_directories(openlegend_diagnostics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(
    openlegend_diagnostics_tests
    PRIVATE openlegend_diagnostics Threads::Threads
)
add_test(NAME openlegend.diagnostics COMMAND openlegend_diagnostics_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME openlegend.reverse_framework
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/research/tools/validate_reverse_framework.py
)
add_test(
    NAME openlegend.build_tool
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_tool_test.py
)

if(TARGET openlegend)
    add_test(
        NAME openlegend.sdl_smoke
        COMMAND openlegend --smoke-test "--data-dir=${OPENLEGEND_GAME_DATA_ROOT}"
    )
    set_tests_properties(
        openlegend.sdl_smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy"
            TIMEOUT 30
    )
endif()
