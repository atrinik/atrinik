include_guard(GLOBAL)

include(FetchContent)

function(atrinik_add_pcpnatpmp)
    # libpcpnatpmp uses generic option names that overlap Atrinik's component
    # switches. Function scope keeps these dependency-only values isolated.
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_CLI_CLIENT OFF)
    set(BUILD_SERVER OFF)
    set(BUILD_TESTS OFF)

    set(pcpnatpmp_patch_args)
    if (MINGW)
        find_package(Git REQUIRED)
        set(pcpnatpmp_patch_args
            PATCH_COMMAND
                ${CMAKE_COMMAND}
                -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                -DSOURCE_DIR=<SOURCE_DIR>
                -DPATCH_FILE=${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/libpcpnatpmp-mingw.patch
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/apply_patch_idempotent.cmake)
    endif ()

    FetchContent_Declare(libpcpnatpmp
        GIT_REPOSITORY https://github.com/libpcpnatpmp/libpcpnatpmp.git
        GIT_TAG 866d283da99f5e98eecff702a8df63e2ae57ffca
        GIT_PROGRESS TRUE
        ${pcpnatpmp_patch_args})
    FetchContent_MakeAvailable(libpcpnatpmp)

    if (NOT TARGET pcpnatpmp)
        message(FATAL_ERROR "libpcpnatpmp did not define its library target")
    endif ()

    add_library(pcpnatpmp::pcpnatpmp ALIAS pcpnatpmp)
endfunction()

atrinik_add_pcpnatpmp()
