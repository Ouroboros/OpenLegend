add_library(openlegend_model STATIC game_snapshot.cpp new_game.cpp)
target_include_directories(
    openlegend_model
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_model
    PUBLIC
        openlegend_compat
        openlegend_random
        openlegend_project_options
        openlegend_project_warnings
)
