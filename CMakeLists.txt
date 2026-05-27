cmake_minimum_required(VERSION 3.20)
project(timepitch_bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# -----------------------------------------------------------------------------
# Build options — enable / disable each library wrapper
# -----------------------------------------------------------------------------
option(BENCH_ENABLE_SIGNALSMITH   "Build Signalsmith Stretch wrapper" ON)
option(BENCH_ENABLE_SOUNDTOUCH    "Build SoundTouch wrapper"          ON)
option(BENCH_ENABLE_RUBBERBAND    "Build Rubber Band wrapper"         ON)
option(BENCH_ENABLE_LIBSAMPLERATE "Build libsamplerate wrapper"       ON)
option(BENCH_ENABLE_R8BRAIN       "Build r8brain-free wrapper"        ON)

# -----------------------------------------------------------------------------
# Dependency-acquisition policy
#
# By default each enabled wrapper resolves its library in this order:
#   1. Local checkout under ${CMAKE_SOURCE_DIR}/external/<name>/
#      (run scripts/fetch-deps.sh to populate this).
#   2. System package, *only if* BENCH_USE_SYSTEM_LIBS=ON. Applies to
#      libsamplerate, soundtouch, rubberband only — signalsmith-stretch
#      and r8brain are not packaged in any distro we know of.
#   3. FetchContent download from upstream Git.
#
# Each dependency emits one status line at configure time identifying which
# source it used.
# -----------------------------------------------------------------------------
option(BENCH_USE_SYSTEM_LIBS "Prefer system-installed packages over FetchContent for libsamplerate / soundtouch / rubberband" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

if(BENCH_USE_SYSTEM_LIBS)
    find_package(PkgConfig QUIET)
    if(NOT PkgConfig_FOUND)
        message(WARNING "BENCH_USE_SYSTEM_LIBS=ON but pkg-config not found; system-lib detection disabled.")
    endif()
endif()

set(_EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/external")

# _try_local_checkout(<fc_name> <dir_name>)
#   If ${_EXTERNAL_DIR}/<dir_name> exists, sets FETCHCONTENT_SOURCE_DIR_<UPPER>
#   to that path so FetchContent uses it instead of cloning. Returns the
#   resolved path in ${out}_PATH (empty if not found).
function(_try_local_checkout fc_name dir_name out_path)
    set(candidate "${_EXTERNAL_DIR}/${dir_name}")
    if(EXISTS "${candidate}/.git" OR EXISTS "${candidate}/CMakeLists.txt" OR EXISTS "${candidate}/README.md")
        string(TOUPPER "${fc_name}" upper)
        set(FETCHCONTENT_SOURCE_DIR_${upper} "${candidate}" CACHE PATH "" FORCE)
        set(${out_path} "${candidate}" PARENT_SCOPE)
    else()
        set(${out_path} "" PARENT_SCOPE)
    endif()
endfunction()

# _print_dep_status(<name> <source> <detail>)
#   Emits a uniform one-line status during configure.
function(_print_dep_status name source detail)
    message(STATUS "[deps] ${name}: ${source} (${detail})")
endfunction()

# -----------------------------------------------------------------------------
# Signalsmith Stretch — MIT, header-mostly. Not packaged anywhere; must be
# fetched or pre-staged into external/signalsmith-stretch/.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_SIGNALSMITH)
    _try_local_checkout(signalsmith_stretch signalsmith-stretch SIGNALSMITH_LOCAL)
    FetchContent_Declare(
        signalsmith_stretch
        GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    if(SIGNALSMITH_LOCAL)
        _print_dep_status("signalsmith-stretch" "local checkout" "${SIGNALSMITH_LOCAL}")
    else()
        _print_dep_status("signalsmith-stretch" "FetchContent"
            "github.com/Signalsmith-Audio/signalsmith-stretch.git @ main")
    endif()
    FetchContent_MakeAvailable(signalsmith_stretch)
    set(BENCH_SIGNALSMITH_LINK_TARGET signalsmith-stretch)
endif()

# -----------------------------------------------------------------------------
# SoundTouch — LGPL. Packaged on most Linux distros as libsoundtouch-dev /
# soundtouch-devel. Falls back to FetchContent of the 2.3.3 tag.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_SOUNDTOUCH)
    set(_soundtouch_resolved FALSE)
    if(BENCH_USE_SYSTEM_LIBS AND PkgConfig_FOUND)
        pkg_check_modules(SYS_SOUNDTOUCH IMPORTED_TARGET QUIET soundtouch)
        if(SYS_SOUNDTOUCH_FOUND)
            _print_dep_status("soundtouch" "system package"
                "pkg-config soundtouch ${SYS_SOUNDTOUCH_VERSION}")
            set(BENCH_SOUNDTOUCH_LINK_TARGET PkgConfig::SYS_SOUNDTOUCH)
            set(_soundtouch_resolved TRUE)
        endif()
    endif()
    if(NOT _soundtouch_resolved)
        _try_local_checkout(soundtouch soundtouch SOUNDTOUCH_LOCAL)
        FetchContent_Declare(
            soundtouch
            GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
            GIT_TAG 2.3.3
            GIT_SHALLOW TRUE
        )
        if(SOUNDTOUCH_LOCAL)
            _print_dep_status("soundtouch" "local checkout" "${SOUNDTOUCH_LOCAL}")
        else()
            _print_dep_status("soundtouch" "FetchContent"
                "codeberg.org/soundtouch/soundtouch.git @ 2.3.3")
        endif()
        set(SOUNDTOUCH_DLL OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(soundtouch)
        set(BENCH_SOUNDTOUCH_LINK_TARGET SoundTouch)
    endif()
endif()

# -----------------------------------------------------------------------------
# Rubber Band — GPL / commercial. No native CMake upstream; we build the
# "single" amalgam either from a system source (rare) or a fetched clone.
# pkg-config usually only locates the prebuilt library; if found, use that.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_RUBBERBAND)
    set(_rubberband_resolved FALSE)
    if(BENCH_USE_SYSTEM_LIBS AND PkgConfig_FOUND)
        pkg_check_modules(SYS_RUBBERBAND IMPORTED_TARGET QUIET rubberband)
        if(SYS_RUBBERBAND_FOUND)
            _print_dep_status("rubberband" "system package"
                "pkg-config rubberband ${SYS_RUBBERBAND_VERSION}")
            set(BENCH_RUBBERBAND_LINK_TARGET PkgConfig::SYS_RUBBERBAND)
            set(_rubberband_resolved TRUE)
        endif()
    endif()
    if(NOT _rubberband_resolved)
        _try_local_checkout(rubberband rubberband RUBBERBAND_LOCAL)
        FetchContent_Declare(
            rubberband
            GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
            GIT_TAG v4.0.0
            GIT_SHALLOW TRUE
        )
        if(RUBBERBAND_LOCAL)
            _print_dep_status("rubberband" "local checkout" "${RUBBERBAND_LOCAL}")
        else()
            _print_dep_status("rubberband" "FetchContent"
                "github.com/breakfastquay/rubberband.git @ v4.0.0")
        endif()
        FetchContent_MakeAvailable(rubberband)
        add_library(rubberband_static STATIC
            ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
        )
        target_include_directories(rubberband_static PUBLIC
            ${rubberband_SOURCE_DIR}
            ${rubberband_SOURCE_DIR}/rubberband
        )
        target_compile_definitions(rubberband_static PRIVATE
            USE_BUILTIN_FFT=1
            NO_THREADING=1
            NO_THREAD_CHECKS=1
        )
        target_compile_options(rubberband_static PRIVATE -Wno-unused -Wno-sign-compare)
        set(BENCH_RUBBERBAND_LINK_TARGET rubberband_static)
    endif()
endif()

# -----------------------------------------------------------------------------
# libsamplerate — BSD-2-Clause. Packaged as libsamplerate0-dev / libsamplerate-devel.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_LIBSAMPLERATE)
    set(_libsamplerate_resolved FALSE)
    if(BENCH_USE_SYSTEM_LIBS AND PkgConfig_FOUND)
        pkg_check_modules(SYS_SAMPLERATE IMPORTED_TARGET QUIET samplerate)
        if(SYS_SAMPLERATE_FOUND)
            _print_dep_status("libsamplerate" "system package"
                "pkg-config samplerate ${SYS_SAMPLERATE_VERSION}")
            set(BENCH_LIBSAMPLERATE_LINK_TARGET PkgConfig::SYS_SAMPLERATE)
            set(_libsamplerate_resolved TRUE)
        endif()
    endif()
    if(NOT _libsamplerate_resolved)
        _try_local_checkout(libsamplerate libsamplerate LIBSAMPLERATE_LOCAL)
        FetchContent_Declare(
            libsamplerate
            GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
            GIT_TAG 0.2.2
            GIT_SHALLOW TRUE
        )
        if(LIBSAMPLERATE_LOCAL)
            _print_dep_status("libsamplerate" "local checkout" "${LIBSAMPLERATE_LOCAL}")
        else()
            _print_dep_status("libsamplerate" "FetchContent"
                "github.com/libsndfile/libsamplerate.git @ 0.2.2")
        endif()
        set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LIBSAMPLERATE_INSTALL  OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(libsamplerate)
        set(BENCH_LIBSAMPLERATE_LINK_TARGET samplerate)
    endif()
endif()

# -----------------------------------------------------------------------------
# r8brain-free — MIT. Not packaged. Must be fetched or pre-staged.
# -----------------------------------------------------------------------------
if(BENCH_ENABLE_R8BRAIN)
    _try_local_checkout(r8brain r8brain R8BRAIN_LOCAL)
    FetchContent_Declare(
        r8brain
        GIT_REPOSITORY https://github.com/avaneev/r8brain-free-src.git
        GIT_TAG version-6.5
        GIT_SHALLOW TRUE
    )
    if(R8BRAIN_LOCAL)
        _print_dep_status("r8brain" "local checkout" "${R8BRAIN_LOCAL}")
    else()
        _print_dep_status("r8brain" "FetchContent"
            "github.com/avaneev/r8brain-free-src.git @ version-6.5")
    endif()
    FetchContent_MakeAvailable(r8brain)
    add_library(r8brain_static STATIC
        ${r8brain_SOURCE_DIR}/r8bbase.cpp
    )
    target_include_directories(r8brain_static PUBLIC ${r8brain_SOURCE_DIR})
    target_compile_options(r8brain_static PRIVATE -Wno-unused -Wno-sign-compare)
    set(BENCH_R8BRAIN_LINK_TARGET r8brain_static)
endif()

# If BENCH_USE_SYSTEM_LIBS was requested but none of the three packageable
# libs were actually located, that's almost certainly user error — flag it.
if(BENCH_USE_SYSTEM_LIBS AND PkgConfig_FOUND
   AND NOT SYS_SOUNDTOUCH_FOUND
   AND NOT SYS_RUBBERBAND_FOUND
   AND NOT SYS_SAMPLERATE_FOUND)
    message(WARNING
        "BENCH_USE_SYSTEM_LIBS=ON but pkg-config found none of "
        "{soundtouch, rubberband, samplerate}. All three fell back to "
        "FetchContent. Install the relevant -dev packages, or unset the "
        "option, to silence this warning.")
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

add_executable(timepitch_bench ${BENCH_SOURCES})

if(BENCH_ENABLE_SIGNALSMITH)
    target_compile_definitions(timepitch_bench PRIVATE BENCH_HAS_SIGNALSMITH=1)
    target_link_libraries(timepitch_bench PRIVATE ${BENCH_SIGNALSMITH_LINK_TARGET})
endif()
if(BENCH_ENABLE_SOUNDTOUCH)
    target_compile_definitions(timepitch_bench PRIVATE BENCH_HAS_SOUNDTOUCH=1)
    target_link_libraries(timepitch_bench PRIVATE ${BENCH_SOUNDTOUCH_LINK_TARGET})
endif()
if(BENCH_ENABLE_RUBBERBAND)
    target_compile_definitions(timepitch_bench PRIVATE BENCH_HAS_RUBBERBAND=1)
    target_link_libraries(timepitch_bench PRIVATE ${BENCH_RUBBERBAND_LINK_TARGET})
endif()
if(BENCH_ENABLE_LIBSAMPLERATE)
    target_compile_definitions(timepitch_bench PRIVATE BENCH_HAS_LIBSAMPLERATE=1)
    target_link_libraries(timepitch_bench PRIVATE ${BENCH_LIBSAMPLERATE_LINK_TARGET})
endif()
if(BENCH_ENABLE_R8BRAIN)
    target_compile_definitions(timepitch_bench PRIVATE BENCH_HAS_R8BRAIN=1)
    target_link_libraries(timepitch_bench PRIVATE ${BENCH_R8BRAIN_LINK_TARGET})
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

_git_rev("${CMAKE_SOURCE_DIR}" BENCH_TIMEPITCHBENCH_GIT_REV)
_git_dirty("${CMAKE_SOURCE_DIR}" BENCH_TIMEPITCHBENCH_GIT_DIRTY)

set(BENCH_SIGNALSMITH_GIT_REV "n/a")
set(BENCH_SIGNALSMITH_TAG     "n/a")
set(BENCH_SOUNDTOUCH_GIT_REV  "n/a")
set(BENCH_SOUNDTOUCH_TAG      "n/a")
set(BENCH_RUBBERBAND_GIT_REV  "n/a")
set(BENCH_RUBBERBAND_TAG      "n/a")

if(BENCH_ENABLE_SIGNALSMITH AND DEFINED signalsmith_stretch_SOURCE_DIR)
    _git_rev("${signalsmith_stretch_SOURCE_DIR}" BENCH_SIGNALSMITH_GIT_REV)
    set(BENCH_SIGNALSMITH_TAG "main")
endif()
if(BENCH_ENABLE_SOUNDTOUCH AND DEFINED soundtouch_SOURCE_DIR)
    _git_rev("${soundtouch_SOURCE_DIR}" BENCH_SOUNDTOUCH_GIT_REV)
    set(BENCH_SOUNDTOUCH_TAG "2.3.3")
endif()
if(BENCH_ENABLE_RUBBERBAND AND DEFINED rubberband_SOURCE_DIR)
    _git_rev("${rubberband_SOURCE_DIR}" BENCH_RUBBERBAND_GIT_REV)
    set(BENCH_RUBBERBAND_TAG "v4.0.0")
endif()

string(TIMESTAMP BENCH_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/build_info.h.in
    ${CMAKE_BINARY_DIR}/generated/build_info.h
    @ONLY
)
target_include_directories(timepitch_bench PRIVATE ${CMAKE_BINARY_DIR}/generated)
