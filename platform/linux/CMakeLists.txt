set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)

add_library(listless_platform_linux
    keyboard.cpp
    terminal.cpp
)

target_link_libraries(listless_platform_linux
    PUBLIC
        listless_core
    PRIVATE
        listless_warnings
        listless_sanitizers
        ${CURSES_LIBRARIES}
)

target_include_directories(listless_platform_linux PUBLIC
    ${CURSES_INCLUDE_DIRS}
)
