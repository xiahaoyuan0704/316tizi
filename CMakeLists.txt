cmake_minimum_required(VERSION 3.21)
project(GimViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(GimViewer WIN32
    src/main.cpp
    src/GimParser.cpp
    src/GimParser.h
)

target_compile_definitions(GimViewer PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if (MSVC)
    target_compile_options(GimViewer PRIVATE /W4 /permissive- /utf-8)
endif()
