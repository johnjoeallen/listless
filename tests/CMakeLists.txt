include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(platform_linux)
endif()
