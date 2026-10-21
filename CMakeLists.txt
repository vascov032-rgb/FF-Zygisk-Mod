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
find_library(egl-lib EGL)
find_library(glesv3-lib GLESv3)

target_link_libraries(arm64-v8a
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    -llog -landroid -lEGL -lGLESv3
)
