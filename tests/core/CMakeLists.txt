add_executable(core_tests
    text_test.cpp
    version_test.cpp
)

target_link_libraries(core_tests PRIVATE
    listless_core
    listless_warnings
    listless_sanitizers
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_tests)
