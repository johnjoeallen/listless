add_executable(platform_linux_tests
    keyboard_test.cpp
    terminal_test.cpp
    viewer_render_test.cpp
)

target_link_libraries(platform_linux_tests PRIVATE
    listless_platform_linux
    listless_warnings
    listless_sanitizers
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(platform_linux_tests)
