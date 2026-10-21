cmake_minimum_required(VERSION 3.22.1)
project(ff_cheat)

set(CMAKE_CXX_STANDARD 17)

# Puxa os arquivos do ImGui
file(GLOB IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/imgui/*.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_android.cpp
)

add_library(arm64-v8a SHARED
    jni.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(arm64-v8a PRIVATE
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/imgui/backends
    ${CMAKE_SOURCE_DIR}
)

find_library(log-lib log)
find_library(android-lib android)
find_library(gles-lib GLESv2)
find_library(egl-lib EGL)

target_link_libraries(arm64-v8a
    ${log-lib}
    ${android-lib}
    ${gles-lib}
    ${egl-lib}
    -llog -landroid -lEGL -lGLESv2
)
