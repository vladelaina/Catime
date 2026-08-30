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

add_executable(shortcut_policy_tests
    tests/shortcut_policy_tests.c
    src/shortcut_policy.c
)
target_include_directories(shortcut_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
add_test(NAME shortcut_policy COMMAND shortcut_policy_tests)

add_executable(shortcut_packaged_tests
    tests/shortcut_packaged_tests.c
    src/startup_shortcut.c
    src/shortcut_shell_packaged.c
    src/utils/string_convert.c
)
target_include_directories(shortcut_packaged_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(shortcut_packaged_tests PRIVATE
    ole32 shell32 uuid shlwapi)
add_test(NAME shortcut_packaged COMMAND shortcut_packaged_tests)

add_executable(time_parser_tests
    tests/time_parser_tests.c
    src/utils/time_parser.c
    src/utils/time_parser_advanced.c
    src/utils/time_format.c
)
target_include_directories(time_parser_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME time_parser COMMAND time_parser_tests)

include(cmake/CatimeHotkeyTests.cmake)
include(cmake/CatimePreviewTests.cmake)

add_executable(tray_animation_playback_tests
    tests/tray_animation_playback_tests.c
    src/tray/tray_animation_playback.c
)
target_include_directories(tray_animation_playback_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME tray_animation_playback COMMAND tray_animation_playback_tests)

add_executable(tray_animation_selection_tests
    tests/tray_animation_selection_tests.c
    src/tray/tray_animation_selection.c
)
target_include_directories(tray_animation_selection_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
add_test(NAME tray_animation_selection COMMAND tray_animation_selection_tests)

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

add_executable(tray_event_protocol_tests
    tests/tray_event_protocol_tests.c
    src/tray/tray_event_protocol.c
)
target_include_directories(tray_event_protocol_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
add_test(NAME tray_event_protocol COMMAND tray_event_protocol_tests)

add_executable(tray_update_policy_tests
    tests/tray_update_policy_tests.c
    src/tray/tray_update_policy.c
)
target_include_directories(tray_update_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME tray_update_policy COMMAND tray_update_policy_tests)

add_executable(tray_recovery_policy_tests
    tests/tray_recovery_policy_tests.c
    src/tray/tray_recovery_policy.c
)
target_include_directories(tray_recovery_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_test(NAME tray_recovery_policy COMMAND tray_recovery_policy_tests)

add_executable(tray_menu_tracking_tests
    tests/tray_menu_tracking_tests.c
    src/tray/tray_menu_tracking.c
)
target_include_directories(tray_menu_tracking_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(tray_menu_tracking_tests PRIVATE user32)
add_test(NAME tray_menu_tracking COMMAND tray_menu_tracking_tests)

add_executable(audio_player_cleanup_tests
    tests/audio_player_cleanup_tests.c
    src/audio_player_cleanup.c
)
target_include_directories(audio_player_cleanup_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
add_test(NAME audio_player_cleanup COMMAND audio_player_cleanup_tests)
set_tests_properties(audio_player_cleanup PROPERTIES TIMEOUT 5)

add_executable(tray_menu_pagination_tests
    tests/tray_menu_pagination_tests.c
    src/tray/tray_menu_pagination.c
)
target_include_directories(tray_menu_pagination_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(tray_menu_pagination_tests PRIVATE gdi32 user32)
add_test(NAME tray_menu_pagination COMMAND tray_menu_pagination_tests)

add_executable(timer_render_cache_tests
    tests/timer_render_cache_tests.c
    src/timer/timer_render_cache.c
)
target_include_directories(timer_render_cache_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(timer_render_cache_tests PRIVATE user32)
add_test(NAME timer_render_cache COMMAND timer_render_cache_tests)

add_executable(pomodoro_suspend_tests
    tests/pomodoro_suspend_tests.c
    src/timer/pomodoro_suspend.c
)
target_include_directories(pomodoro_suspend_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/timer"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(pomodoro_suspend_tests PRIVATE user32)
add_test(NAME pomodoro_suspend COMMAND pomodoro_suspend_tests)

add_executable(render_retry_tests
    tests/render_retry_tests.c
    src/utils/render_retry.c
)
target_include_directories(render_retry_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(render_retry_tests PRIVATE user32)
add_test(NAME render_retry COMMAND render_retry_tests)

add_executable(system_monitor_snapshot_tests
    tests/system_monitor_snapshot_tests.c
    src/system_monitor.c
    src/system_monitor_basic.c
    src/system_monitor_network_api.c
    src/system_monitor_network_sample.c
    src/system_monitor_network_worker.c
    src/system_monitor_state.c
)
target_include_directories(system_monitor_snapshot_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(system_monitor_snapshot_tests PRIVATE iphlpapi)
add_test(NAME system_monitor_snapshot COMMAND system_monitor_snapshot_tests)

add_executable(tray_metric_sync_tests
    tests/tray_metric_sync_tests.c
    src/tray/tray_metric_sync.c
)
target_include_directories(tray_metric_sync_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tray"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
add_test(NAME tray_metric_sync COMMAND tray_metric_sync_tests)

add_executable(taskbar_monitor_compositor_tests
    tests/taskbar_monitor_compositor_tests.c
    src/drawing/system_ui_font.c
    src/taskbar_monitor/taskbar_monitor_compositor.c
    src/taskbar_monitor/taskbar_monitor_layout.c
)
target_include_directories(taskbar_monitor_compositor_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(taskbar_monitor_compositor_tests PRIVATE gdi32 user32)
add_test(NAME taskbar_monitor_compositor
    COMMAND taskbar_monitor_compositor_tests)

add_executable(taskbar_monitor_parent_tests
    tests/taskbar_monitor_parent_tests.c
    src/taskbar_monitor/taskbar_monitor_parent.c
)
target_include_directories(taskbar_monitor_parent_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(taskbar_monitor_parent_tests PRIVATE user32)
add_test(NAME taskbar_monitor_parent
    COMMAND taskbar_monitor_parent_tests)

add_executable(taskbar_monitor_policy_tests
    tests/taskbar_monitor_policy_tests.c
    src/taskbar_monitor/taskbar_monitor_policy.c
)
target_include_directories(taskbar_monitor_policy_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
add_test(NAME taskbar_monitor_policy
    COMMAND taskbar_monitor_policy_tests)

add_executable(taskbar_monitor_recovery_tests
    tests/taskbar_monitor_recovery_tests.c
    src/taskbar_monitor/taskbar_monitor_parent.c
    src/taskbar_monitor/taskbar_monitor_recovery.c
)
target_include_directories(taskbar_monitor_recovery_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(taskbar_monitor_recovery_tests PRIVATE user32)
add_test(NAME taskbar_monitor_recovery
    COMMAND taskbar_monitor_recovery_tests)

add_executable(taskbar_monitor_placement_tests
    tests/taskbar_monitor_placement_tests.c
    src/taskbar_monitor/taskbar_monitor_placement.c
)
target_include_directories(taskbar_monitor_placement_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
add_test(NAME taskbar_monitor_placement
    COMMAND taskbar_monitor_placement_tests)

add_executable(tray_percent_font_tests
    tests/tray_percent_font_tests.c
    src/drawing/system_ui_font.c
    src/tray/tray_animation_percent.c
    src/tray/tray_animation_percent_font.c
    src/tray/tray_animation_percent_icons.c
    src/tray/tray_animation_percent_text.c
)
target_include_directories(tray_percent_font_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(tray_percent_font_tests PRIVATE gdi32 user32)
add_test(NAME tray_percent_font COMMAND tray_percent_font_tests)

set(_catime_test_targets
    window_placement_tests
    startup_policy_tests
    startup_shortcut_tests
    shortcut_policy_tests
    shortcut_packaged_tests
    time_parser_tests
    menu_preview_lifecycle_tests
    hotkey_config_tests
    tray_animation_playback_tests
    tray_animation_selection_tests
    tray_animation_speed_input_tests
    tray_animation_timer_tests
    tray_icon_lifetime_tests
    tray_hover_cache_tests
    tray_event_protocol_tests
    tray_update_policy_tests
    tray_recovery_policy_tests
    tray_menu_tracking_tests
    audio_player_cleanup_tests
    tray_menu_pagination_tests
    timer_render_cache_tests
    pomodoro_suspend_tests
    render_retry_tests
    system_monitor_snapshot_tests
    tray_metric_sync_tests
    taskbar_monitor_compositor_tests
    taskbar_monitor_parent_tests
    taskbar_monitor_policy_tests
    taskbar_monitor_recovery_tests
    taskbar_monitor_placement_tests
    tray_percent_font_tests
)

if(MSVC)
    foreach(_catime_test_target IN LISTS _catime_test_targets)
        target_compile_options(${_catime_test_target} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:/utf-8>
        )
    endforeach()
endif()

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

unset(_catime_test_targets)
unset(_catime_test_target)
