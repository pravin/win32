cmake_minimum_required(VERSION 3.20)
project(DrawLite C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# WIN32 makes this a GUI program (no console window)
add_executable(drawlite WIN32 src/main.c)

# We use the Unicode flavour of the Windows API, and target Windows 10 and up
target_compile_definitions(drawlite PRIVATE UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)

if(MSVC)
    target_compile_options(drawlite PRIVATE /W4 /utf-8)
else()
    target_compile_options(drawlite PRIVATE -Wall -Wextra)
    # wWinMain needs -municode with MinGW
    target_link_options(drawlite PRIVATE -municode)
endif()

target_link_libraries(drawlite PRIVATE user32)
