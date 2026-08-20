add_library(listless_core
    AppActions.cpp
    Color.cpp
    ColorPairTable.cpp
    Directory.cpp
    FileManager.cpp
    FileManagerRender.cpp
    Glob.cpp
    Key.cpp
    LineEdit.cpp
    Search.cpp
    Style.cpp
    SyntaxHighlight.cpp
    Text.cpp
    Version.cpp
    Viewer.cpp
    ViewerRender.cpp
)

target_include_directories(listless_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(listless_core PRIVATE
    listless_warnings
    listless_sanitizers
)
