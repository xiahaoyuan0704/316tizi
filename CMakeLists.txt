cmake_minimum_required(VERSION 3.16)
project(gim_toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gim_toolkit
    src/main.cpp
    src/gim_model.cpp
    src/gim_parser.cpp
    src/software_renderer.cpp
)

target_include_directories(gim_toolkit PRIVATE include)
