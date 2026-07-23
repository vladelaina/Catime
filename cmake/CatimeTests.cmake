include(CTest)
if(NOT BUILD_TESTING)
    return()
endif()

add_executable(window_placement_tests
    tests/window_placement_tests.c
    src/window/window_placement.c
)
target_include_directories(window_placement_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME window_placement COMMAND window_placement_tests)

add_executable(startup_policy_tests
    tests/startup_policy_tests.c
    src/startup_policy.c
)
target_include_directories(startup_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME startup_policy COMMAND startup_policy_tests)

add_executable(startup_shortcut_tests
    tests/startup_shortcut_tests.c
    src/startup_shortcut.c
)
target_include_directories(startup_shortcut_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(startup_shortcut_tests PRIVATE ole32 uuid shlwapi)
add_test(NAME startup_shortcut COMMAND startup_shortcut_tests)

add_executable(tray_animation_playback_tests
    tests/tray_animation_playback_tests.c
    src/tray/tray_animation_playback.c
)
target_include_directories(tray_animation_playback_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME tray_animation_playback COMMAND tray_animation_playback_tests)

add_executable(tray_animation_speed_input_tests
    tests/tray_animation_speed_input_tests.c
    src/tray/tray_animation_speed_input.c
)
target_include_directories(tray_animation_speed_input_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
add_test(NAME tray_animation_speed_input COMMAND tray_animation_speed_input_tests)

add_executable(tray_animation_timer_tests
    tests/tray_animation_timer_tests.c
    src/tray/tray_animation_timer.c
    src/tray/tray_animation_rate.c
)
target_include_directories(tray_animation_timer_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(tray_animation_timer_tests PRIVATE user32 winmm)
add_test(NAME tray_animation_timer COMMAND tray_animation_timer_tests)

add_executable(tray_icon_lifetime_tests
    tests/tray_icon_lifetime_tests.c
    src/tray/tray_icon_lifetime.c
)
target_include_directories(tray_icon_lifetime_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(tray_icon_lifetime_tests PRIVATE gdi32 user32)
add_test(NAME tray_icon_lifetime COMMAND tray_icon_lifetime_tests)

add_executable(tray_hover_cache_tests
    tests/tray_hover_cache_tests.c
    src/tray/tray_hover_cache.c
)
target_include_directories(tray_hover_cache_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(tray_hover_cache_tests PRIVATE user32)
add_test(NAME tray_hover_cache COMMAND tray_hover_cache_tests)

add_executable(timer_render_cache_tests
    tests/timer_render_cache_tests.c
    src/timer/timer_render_cache.c
)
target_include_directories(timer_render_cache_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(timer_render_cache_tests PRIVATE user32)
add_test(NAME timer_render_cache COMMAND timer_render_cache_tests)

add_executable(render_retry_tests
    tests/render_retry_tests.c
    src/utils/render_retry.c
)
target_include_directories(render_retry_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(render_retry_tests PRIVATE user32)
add_test(NAME render_retry COMMAND render_retry_tests)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(animation_test_target IN ITEMS
            tray_animation_playback_tests
            tray_animation_speed_input_tests
            tray_animation_timer_tests
            tray_icon_lifetime_tests
            tray_hover_cache_tests)
        target_compile_options(${animation_test_target} PRIVATE -O2 -ffast-math)
    endforeach()
endif()
