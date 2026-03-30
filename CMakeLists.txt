cmake_minimum_required(VERSION 3.16)
project(gim_toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(gim_core
    src/gim_model.cpp
    src/gim_parser.cpp
    src/software_renderer.cpp
)

target_include_directories(gim_core PUBLIC include)

add_executable(gim_toolkit src/main.cpp)
target_link_libraries(gim_toolkit PRIVATE gim_core)

option(GIM_ENABLE_STUDIO "Build realtime studio (OpenGL + ImGui)" ON)

if(GIM_ENABLE_STUDIO)
    include(FetchContent)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
    )
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()

    find_package(OpenGL REQUIRED)

    add_library(imgui_glfw_opengl STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_include_directories(imgui_glfw_opengl PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_glfw_opengl PUBLIC glfw OpenGL::GL)

    add_executable(gim_studio
        src/studio_main.cpp
        src/gl_viewer.cpp
    )
    target_link_libraries(gim_studio PRIVATE gim_core imgui_glfw_opengl glfw OpenGL::GL)
    target_include_directories(gim_studio PRIVATE include)

    target_compile_definitions(gim_studio PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
endif()
