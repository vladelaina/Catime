add_executable(menu_preview_lifecycle_tests
    tests/menu_preview_lifecycle_tests.c
    src/menu_preview.c
    src/menu_preview_apply.c
    src/window_procedure/window_preview_policy.c
)
target_include_directories(menu_preview_lifecycle_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(menu_preview_lifecycle_tests PRIVATE user32)
add_test(NAME menu_preview_lifecycle COMMAND menu_preview_lifecycle_tests)
