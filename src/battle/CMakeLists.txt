add_library(openlegend_battle STATIC battle_data.cpp battle_setup.cpp)
target_include_directories(
    openlegend_battle
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(
    openlegend_battle
    PUBLIC
        openlegend_model
        openlegend_resource
        openlegend_project_options
        openlegend_project_warnings
)
