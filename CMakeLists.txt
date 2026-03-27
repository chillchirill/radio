cmake_minimum_required(VERSION 3.20)

project(radiomaster_reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(radiomaster_reader
    main.cpp
)

target_link_libraries(radiomaster_reader PRIVATE winmm)

if(MSVC)
    target_compile_options(radiomaster_reader PRIVATE /W4 /permissive-)
else()
    target_compile_options(radiomaster_reader PRIVATE -Wall -Wextra -Wpedantic)
endif()
