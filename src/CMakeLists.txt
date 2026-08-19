add_library(listless_core
    color.cpp
    color_pair_table.cpp
    directory.cpp
    file_manager.cpp
    glob.cpp
    key.cpp
    search.cpp
    style.cpp
    text.cpp
    version.cpp
    viewer.cpp
    viewer_render.cpp
)

target_include_directories(listless_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(listless_core PRIVATE
    listless_warnings
    listless_sanitizers
)
