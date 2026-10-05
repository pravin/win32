cmake_minimum_required(VERSION 3.20)
project(DrawLite C RC)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# WIN32 makes this a GUI program (no console window)
add_executable(drawlite WIN32
    src/main.c
    src/mainwindow.c
    src/aboutdlg.c
    src/palette.c
    src/canvas.c
    src/tools.c
    src/uiutil.c
    res/drawlite.rc
)

# Lets the resource compiler find resource.h, the icon and the manifest
target_include_directories(drawlite PRIVATE src res)

# We use the Unicode flavour of the Windows API, and target Windows 10 and up
target_compile_definitions(drawlite PRIVATE UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)

# The .rc file includes the manifest, so rebuild when these change
set_source_files_properties(res/drawlite.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/res/app.manifest;${CMAKE_CURRENT_SOURCE_DIR}/res/drawlite.ico;${CMAKE_CURRENT_SOURCE_DIR}/src/resource.h")

if(MSVC)
    target_compile_options(drawlite PRIVATE /W4 /utf-8)
    # Our manifest is inside the .rc file, so stop the linker making its own
    target_link_options(drawlite PRIVATE /MANIFEST:NO)
else()
    target_compile_options(drawlite PRIVATE -Wall -Wextra)
    # wWinMain needs -municode with MinGW
    target_link_options(drawlite PRIVATE -municode)
endif()

target_link_libraries(drawlite PRIVATE user32 gdi32 comctl32)
