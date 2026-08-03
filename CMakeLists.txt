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

set(PACKAGE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(PACKAGE_VERSION_MINOR "${CMAKE_MATCH_2}")
if ("${CMAKE_MATCH_4}" STREQUAL "")
    set(PACKAGE_VERSION_PATCH 0)
else ()
    set(PACKAGE_VERSION_PATCH "${CMAKE_MATCH_4}")
endif ()

include_directories(common)
include_directories(${CMAKE_BINARY_DIR}/common)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcommon)
endif ()

# Add some useful compile flags.
add_definitions(-Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -D__USE_MINGW_ANSI_STDIO=0 -std=gnu99)
add_definitions(${CUSTOM_WARNINGS})

if (NOT MINGW)
    if (ENABLE_STACK_PROTECTOR)
        add_definitions(-fstack-protector-all -Wstack-protector)
    else ()
        add_definitions(-fno-stack-protector)
    endif ()
endif ()

if (ENABLE_EXTRA_WARNINGS)
    # Non-macro identifiers inside #if outside of defined()
    add_definitions(-Wundef)
    # sizeof() on function types or void
    add_definitions(-Wpointer-arith)
    # Casting function types to non-matching types
    add_definitions(-Wbad-function-cast)
    # extern declaration within a function
    add_definitions(-Wnested-externs)
    # Dead code (removed in newer versions of GCC, might still be in clang)
    add_definitions(-Wunreachable-code)
    # Additional format checks for printf/scanf/etc not enabled by -Wall
    add_definitions(-Wformat=2 -Wno-format-nonliteral)
    # Declarations like int i = i
    add_definitions(-Winit-self)
    # Local variables or types that shadow another variable, parameter, type,
    # built-in function, etc.
    add_definitions(-Wshadow)
    # Global function defined without a previous declaration.
    add_definitions(-Wmissing-declarations)
    # Variable that may be clobbered by a setjmp call
    add_definitions(-Wuninitialized)
    # Testing floating point values for equality
    add_definitions(-Wfloat-equal)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # Suspicious uses of logical operators in expressions
        add_definitions(-Wlogical-op)
        # Pointer casting with increased alignment (char * -> int *)
        add_definitions(-Wcast-align)
    endif ()
endif ()

if (ENABLE_AGGRESSIVE_WARNINGS)
    # Casting const char * to char *
    add_definitions(-Wcast-qual)
    # Assigning signed int to unsigned int
    add_definitions(-Wsign-conversion)
    # Loop indices that could potentially overflow
    add_definitions(-Wunsafe-loop-optimizations)
    # Redundant (multiple same) declarations
    add_definitions(-Wredundant-decls)
endif ()

# Turn warnings into errors?
if (ENABLE_WARNING_ERRORS)
    add_definitions(-Werror)
endif ()

if (MINGW)
    add_definitions(-DNOCRYPT)
    set(PLUGIN_SUFFIX ".dll")
else ()
    # Compiling on GNU/Linux.
    set(LINUX true)
    set(PLUGIN_SUFFIX ".so")
endif ()

add_subdirectory(common/toolkit)

if (BUILD_SERVER)
    add_subdirectory(server)
endif ()

if (BUILD_CLIENT)
    add_subdirectory(client)
endif ()

if (NOT BUILD_SERVER AND NOT BUILD_CLIENT)
    message(FATAL_ERROR "At least one of BUILD_SERVER or BUILD_CLIENT must be enabled")
endif ()
