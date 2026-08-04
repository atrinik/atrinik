cmake_minimum_required(VERSION 3.21)
project(atrinik C)

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

# Include build configuration.
include(build.config)

if (EXISTS build.user)
    include(build.user)
endif ()

if (DEFINED ENV{ATRINIK_PACKAGE_VERSION} AND
        NOT "$ENV{ATRINIK_PACKAGE_VERSION}" STREQUAL "")
    set(PACKAGE_VERSION "$ENV{ATRINIK_PACKAGE_VERSION}")
endif ()

if (NOT PACKAGE_VERSION MATCHES
        "^([0-9]+)\\.([0-9]+)(\\.([0-9]+))?$")
    message(FATAL_ERROR
        "PACKAGE_VERSION must have the form MAJOR.MINOR or MAJOR.MINOR.PATCH")
endif ()

# Preserve the regex captures before later MATCHES expressions overwrite
# CMake's shared CMAKE_MATCH_<n> variables.
set(PACKAGE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(PACKAGE_VERSION_MINOR "${CMAKE_MATCH_2}")
if ("${CMAKE_MATCH_4}" STREQUAL "")
    set(PACKAGE_VERSION_PATCH 0)
else ()
    set(PACKAGE_VERSION_PATCH "${CMAKE_MATCH_4}")
endif ()

include(CTest)

add_library(atrinik-build-options INTERFACE)
target_compile_features(atrinik-build-options INTERFACE c_std_17)
target_compile_definitions(atrinik-build-options INTERFACE _GNU_SOURCE)

if (MINGW)
    target_compile_definitions(atrinik-build-options INTERFACE
        HAVE_STRUCT_TIMESPEC
        NOCRYPT
        __USE_MINGW_ANSI_STDIO=0)
else ()
    set(LINUX true)
endif ()

add_library(atrinik-warnings INTERFACE)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(atrinik-warnings INTERFACE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -fno-common)

    if (CUSTOM_WARNINGS)
        target_compile_options(atrinik-warnings INTERFACE ${CUSTOM_WARNINGS})
    endif ()

    if (ENABLE_EXTRA_WARNINGS)
        target_compile_options(atrinik-warnings INTERFACE
            -Wundef
            -Wpointer-arith
            -Wbad-function-cast
            -Wnested-externs
            -Wunreachable-code
            -Wformat=2
            -Wno-format-nonliteral
            -Winit-self
            -Wshadow
            -Wmissing-declarations
            -Wuninitialized
            -Wfloat-equal)

        if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(atrinik-warnings INTERFACE
                -Wlogical-op
                -Wcast-align)
        endif ()
    endif ()

    if (ENABLE_AGGRESSIVE_WARNINGS)
        target_compile_options(atrinik-warnings INTERFACE
            -Wcast-qual
            -Wsign-conversion
            -Wunsafe-loop-optimizations
            -Wredundant-decls)
    endif ()

    if (ENABLE_WARNING_ERRORS)
        target_compile_options(atrinik-warnings INTERFACE -Werror)
    endif ()
endif ()

add_library(atrinik-hardening INTERFACE)
if (NOT MINGW AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if (ENABLE_STACK_PROTECTOR)
        target_compile_options(atrinik-hardening INTERFACE
            -fstack-protector-all
            -Wstack-protector)
    else ()
        target_compile_options(atrinik-hardening INTERFACE
            -fno-stack-protector)
    endif ()
endif ()

add_library(atrinik-sanitizers INTERFACE)
if (ENABLE_SANITIZERS)
    if (MINGW OR NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "ENABLE_SANITIZERS requires a native GCC or Clang build")
    endif ()

    target_compile_options(atrinik-sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer)
    target_link_options(atrinik-sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer)
endif ()

add_library(atrinik-project-options INTERFACE)
target_link_libraries(atrinik-project-options INTERFACE
    atrinik-build-options
    atrinik-warnings
    atrinik-hardening
    atrinik-sanitizers)

function(atrinik_configure_target target)
    target_link_libraries(${target} PRIVATE atrinik-project-options)
endfunction()

if (BUILD_SERVER)
    include(cmake/pcpnatpmp.cmake)
endif ()

if (MINGW)
    set(PLUGIN_SUFFIX ".dll")
else ()
    set(PLUGIN_SUFFIX ".so")
endif ()

add_subdirectory(common/toolkit)

if (BUILD_E2E_TESTS AND NOT MINGW)
    add_subdirectory(tests/e2e)
endif ()

if (BUILD_SERVER)
    add_subdirectory(server)
endif ()

if (BUILD_CLIENT)
    add_subdirectory(client)
endif ()

if (NOT BUILD_SERVER AND NOT BUILD_CLIENT)
    message(FATAL_ERROR "At least one of BUILD_SERVER or BUILD_CLIENT must be enabled")
endif ()
