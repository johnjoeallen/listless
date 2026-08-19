add_executable(core_tests
    color_pair_table_test.cpp
    color_test.cpp
    directory_test.cpp
    file_manager_test.cpp
    glob_test.cpp
    key_test.cpp
    search_test.cpp
    text_test.cpp
    version_test.cpp
    viewer_test.cpp
)

target_link_libraries(core_tests PRIVATE
    listless_core
    listless_warnings
    listless_sanitizers
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_tests)
