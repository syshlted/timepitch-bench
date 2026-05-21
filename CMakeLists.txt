cmake_minimum_required(VERSION 3.20)
project(dsp_bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

option(BENCH_ENABLE_SIGNALSMITH "Build Signalsmith Stretch wrapper" ON)
option(BENCH_ENABLE_SOUNDTOUCH  "Build SoundTouch wrapper"          ON)
option(BENCH_ENABLE_RUBBERBAND  "Build Rubber Band wrapper"         ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# -----------------------------------------------------------------------------
# Signalsmith Stretch — MIT, single-header / small lib
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_SIGNALSMITH)
    FetchContent_Declare(
        signalsmith_stretch
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(signalsmith_stretch)
    # Upstream provides target `signalsmith-stretch` (with hyphen) that already
    # pulls in signalsmith-linear and the correct include paths.
endif()

# -----------------------------------------------------------------------------
# SoundTouch — LGPL, has CMakeLists
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_SOUNDTOUCH)
    FetchContent_Declare(
        soundtouch
        GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
        GIT_TAG 2.3.3
        GIT_SHALLOW TRUE
    )
    set(SOUNDTOUCH_DLL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(soundtouch)
endif()

# -----------------------------------------------------------------------------
# Rubber Band — LGPL / commercial. No native CMake; build the "single" amalgam.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_RUBBERBAND)
    FetchContent_Declare(
        rubberband
        GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
        GIT_TAG v4.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(rubberband)

    # The repo ships single/RubberBandSingle.cpp as an amalgamation for embedding.
    add_library(rubberband_static STATIC
        ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
    )
    target_include_directories(rubberband_static PUBLIC
        ${rubberband_SOURCE_DIR}
        ${rubberband_SOURCE_DIR}/rubberband
    )
    # The amalgam expects either USE_BUILTIN_FFT or one of the system FFT macros.
    # Built-in keeps the build self-contained at the cost of some CPU performance.
    target_compile_definitions(rubberband_static PRIVATE
        USE_BUILTIN_FFT=1
        NO_THREADING=1
        NO_THREAD_CHECKS=1
    )
    target_compile_options(rubberband_static PRIVATE -Wno-unused -Wno-sign-compare)
endif()

# -----------------------------------------------------------------------------
# Benchmark executable
# -----------------------------------------------------------------------------
set(BENCH_SOURCES
    src/main.cpp
    src/signals.cpp
    src/metrics.cpp
    src/runner.cpp
    src/fft.cpp
)

if(BENCH_ENABLE_SIGNALSMITH)
    list(APPEND BENCH_SOURCES src/stretcher_signalsmith.cpp)
endif()
if(BENCH_ENABLE_SOUNDTOUCH)
    list(APPEND BENCH_SOURCES src/stretcher_soundtouch.cpp)
endif()
if(BENCH_ENABLE_RUBBERBAND)
    list(APPEND BENCH_SOURCES src/stretcher_rubberband.cpp)
endif()

add_executable(dsp_bench ${BENCH_SOURCES})

if(BENCH_ENABLE_SIGNALSMITH)
    target_compile_definitions(dsp_bench PRIVATE BENCH_HAS_SIGNALSMITH=1)
    target_link_libraries(dsp_bench PRIVATE signalsmith-stretch)
endif()
if(BENCH_ENABLE_SOUNDTOUCH)
    target_compile_definitions(dsp_bench PRIVATE BENCH_HAS_SOUNDTOUCH=1)
    target_link_libraries(dsp_bench PRIVATE SoundTouch)
endif()
if(BENCH_ENABLE_RUBBERBAND)
    target_compile_definitions(dsp_bench PRIVATE BENCH_HAS_RUBBERBAND=1)
    target_link_libraries(dsp_bench PRIVATE rubberband_static)
endif()
