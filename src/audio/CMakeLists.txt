find_package(Threads REQUIRED)

add_library(
    openlegend_audio
    legacy_audio.cpp
    legacy_audio_worker.cpp
    audio_mixer.cpp
)
target_include_directories(openlegend_audio PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    openlegend_audio
    PUBLIC openlegend_project_options openlegend_resource
    PRIVATE ADLMIDI openlegend_project_warnings Threads::Threads
)
