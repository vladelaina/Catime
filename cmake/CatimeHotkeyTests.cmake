add_executable(hotkey_config_tests
    tests/hotkey_config_tests.c
    src/config/config_hotkey.c
)
target_include_directories(hotkey_config_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
add_test(NAME hotkey_config COMMAND hotkey_config_tests)
