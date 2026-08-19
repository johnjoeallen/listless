add_library(listless_core
    text.cpp
    version.cpp
)

target_include_directories(listless_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(listless_core PRIVATE
    listless_warnings
    listless_sanitizers
)
