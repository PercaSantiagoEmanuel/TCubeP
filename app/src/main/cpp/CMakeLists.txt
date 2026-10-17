cmake_minimum_required(VERSION 3.22.1)
project("neondodge" C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(PLATFORM "Android" CACHE STRING "" FORCE)

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

# Auto-patch pentru NDK 28+
set(RCORE_ANDROID_FILE "${raylib_SOURCE_DIR}/src/platforms/rcore_android.c")
if(EXISTS "${RCORE_ANDROID_FILE}")
    file(READ "${RCORE_ANDROID_FILE}" RCORE_CONTENT)
    string(REPLACE "ALooper_pollAll" "ALooper_pollOnce" RCORE_CONTENT "${RCORE_CONTENT}")
    file(WRITE "${RCORE_ANDROID_FILE}" "${RCORE_CONTENT}")
endif()

add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

add_library(
    neondodge
    SHARED
    main.cpp
    GameScreen.cpp
    Renderer.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(gles-lib GLESv2)
find_library(egl-lib EGL)

target_link_libraries(
    neondodge
    PRIVATE
    raylib
    native_app_glue
    ${log-lib}
    ${android-lib}
    ${gles-lib}
    ${egl-lib}
    OpenSLES
    m
    z
)

# LINIA CARE REZOLVĂ EROAREA: FORȚEAZĂ PĂSTRAREA FUNCȚIEI PENTRU ANDROID
target_link_options(neondodge PRIVATE "-u" "ANativeActivity_onCreate")