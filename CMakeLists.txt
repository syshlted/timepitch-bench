cmake_minimum_required(VERSION 3.20)
project(dsp_bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

option(BENCH_ENABLE_SIGNALSMITH   "Build Signalsmith Stretch wrapper" ON)
option(BENCH_ENABLE_SOUNDTOUCH    "Build SoundTouch wrapper"          ON)
option(BENCH_ENABLE_RUBBERBAND    "Build Rubber Band wrapper"         ON)
option(BENCH_ENABLE_LIBSAMPLERATE "Build libsamplerate wrapper (import-path resampler, D-016)" ON)
option(BENCH_ENABLE_R8BRAIN       "Build r8brain-free wrapper (import-path resampler, A/B vs libsamplerate)" ON)

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
# libsamplerate — BSD-2; offline resampler for the .mloop import path (D-016).
# Not a stretcher candidate; parallel slot.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_LIBSAMPLERATE)
    set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LIBSAMPLERATE_INSTALL  OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libsamplerate
        GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
        GIT_TAG 0.2.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(libsamplerate)
endif()

# -----------------------------------------------------------------------------
# r8brain-free — MIT. version-6.5 still ships an r8bbase.cpp with out-of-line
# static definitions (cache state for the fractional-delay filter bank).
# Compile it into a small static lib. Newer trunk is header-only but we pin
# to a tagged release for reproducibility.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_R8BRAIN)
    FetchContent_Declare(
        r8brain
        GIT_REPOSITORY https://github.com/avaneev/r8brain-free-src.git
        GIT_TAG version-6.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(r8brain)
    add_library(r8brain_static STATIC
        ${r8brain_SOURCE_DIR}/r8bbase.cpp
    )
    target_include_directories(r8brain_static PUBLIC ${r8brain_SOURCE_DIR})
    target_compile_options(r8brain_static PRIVATE -Wno-unused -Wno-sign-compare)
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
    src/provenance.cpp
    src/report.cpp
    src/resampler_registry.cpp
    src/resample_runner.cpp
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
if(BENCH_ENABLE_LIBSAMPLERATE)
    list(APPEND BENCH_SOURCES src/resampler_libsamplerate.cpp)
endif()
if(BENCH_ENABLE_R8BRAIN)
    list(APPEND BENCH_SOURCES src/resampler_r8brain.cpp)
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
if(BENCH_ENABLE_LIBSAMPLERATE)
    target_compile_definitions(dsp_bench PRIVATE BENCH_HAS_LIBSAMPLERATE=1)
    target_link_libraries(dsp_bench PRIVATE samplerate)
endif()
if(BENCH_ENABLE_R8BRAIN)
    target_compile_definitions(dsp_bench PRIVATE BENCH_HAS_R8BRAIN=1)
    target_link_libraries(dsp_bench PRIVATE r8brain_static)
endif()

# -----------------------------------------------------------------------------
# Provenance: capture git revs and build metadata into build_info.h
# -----------------------------------------------------------------------------
find_package(Git QUIET)

function(_git_rev dir out_var)
    set(${out_var} "unknown" PARENT_SCOPE)
    if(NOT GIT_EXECUTABLE OR NOT IS_DIRECTORY "${dir}")
        return()
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${dir}" rev-parse --short HEAD
        OUTPUT_VARIABLE rev OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE rc ERROR_QUIET)
    if(rc EQUAL 0 AND rev)
        set(${out_var} "${rev}" PARENT_SCOPE)
    endif()
endfunction()

function(_git_dirty dir out_var)
    set(${out_var} "false" PARENT_SCOPE)
    if(NOT GIT_EXECUTABLE OR NOT IS_DIRECTORY "${dir}")
        return()
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${dir}" status --porcelain -- "${dir}"
        OUTPUT_VARIABLE st OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE rc ERROR_QUIET)
    if(rc EQUAL 0 AND st)
        set(${out_var} "true" PARENT_SCOPE)
    endif()
endfunction()

_git_rev("${CMAKE_SOURCE_DIR}" BENCH_DSPBENCH_GIT_REV)
_git_dirty("${CMAKE_SOURCE_DIR}" BENCH_DSPBENCH_GIT_DIRTY)

set(BENCH_SIGNALSMITH_GIT_REV "n/a")
set(BENCH_SIGNALSMITH_TAG     "n/a")
set(BENCH_SOUNDTOUCH_GIT_REV  "n/a")
set(BENCH_SOUNDTOUCH_TAG      "n/a")
set(BENCH_RUBBERBAND_GIT_REV  "n/a")
set(BENCH_RUBBERBAND_TAG      "n/a")

if(BENCH_ENABLE_SIGNALSMITH)
    _git_rev("${signalsmith_stretch_SOURCE_DIR}" BENCH_SIGNALSMITH_GIT_REV)
    set(BENCH_SIGNALSMITH_TAG "main")
endif()
if(BENCH_ENABLE_SOUNDTOUCH)
    _git_rev("${soundtouch_SOURCE_DIR}" BENCH_SOUNDTOUCH_GIT_REV)
    set(BENCH_SOUNDTOUCH_TAG "2.3.3")
endif()
if(BENCH_ENABLE_RUBBERBAND)
    _git_rev("${rubberband_SOURCE_DIR}" BENCH_RUBBERBAND_GIT_REV)
    set(BENCH_RUBBERBAND_TAG "v4.0.0")
endif()

string(TIMESTAMP BENCH_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/build_info.h.in
    ${CMAKE_BINARY_DIR}/generated/build_info.h
    @ONLY
)
target_include_directories(dsp_bench PRIVATE ${CMAKE_BINARY_DIR}/generated)
