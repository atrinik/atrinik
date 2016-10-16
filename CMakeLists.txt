project(atrinik C)
cmake_minimum_required(VERSION 2.6)

# Include build configuration.
include(build.config)

if (EXISTS build.user)
    include(build.user)
endif ()

include_directories(common)

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
    add_definitions(-DCURL_STATICLIB -DPTW32_STATIC_LIB -DBGDWIN32 -DNONDLL)
    add_definitions(-DNOCRYPT)
    set(CURL_STATICLIB true)
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc")
    set(PLUGIN_SUFFIX ".dll")
    set(CMAKE_PREFIX_PATH "C:\\MinGW\\msys\\1.0;C:\\MinGW")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.so;.dll.a")
else ()
    # Compiling on GNU/Linux.
    set(LINUX true)
    set(PLUGIN_SUFFIX ".so")
endif ()

add_subdirectory(common/toolkit)
add_subdirectory(server)
add_subdirectory(client)
