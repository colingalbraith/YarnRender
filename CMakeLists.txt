cmake_minimum_required(VERSION 3.20)
project(YarnRender LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(YarnRender
    YarnRender.cpp
    Globals.cpp
    YarnMath.cpp
    YarnGeometry.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

target_include_directories(YarnRender PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/imgui)
target_compile_definitions(YarnRender PRIVATE GL_SILENCE_DEPRECATION CY_NO_IMMINTRIN_H NOMINMAX)
target_link_libraries(YarnRender PRIVATE GLEW::GLEW glfw OpenGL::GL)
