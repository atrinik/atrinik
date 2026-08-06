if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "BUILD_FUZZING requires Clang's libFuzzer runtime")
endif ()

set(ATRINIK_FUZZ_TARGETS
    packet
    envelope
    dispatch
    configuration
    asset
    map
    rendezvous)
set(ATRINIK_FUZZ_EXECUTABLES)

foreach (name IN LISTS ATRINIK_FUZZ_TARGETS)
    set(target "fuzz-${name}")
    list(APPEND ATRINIK_FUZZ_EXECUTABLES ${target})
    add_executable(${target} protocol_fuzzer.c)
    target_compile_definitions(${target} PRIVATE "ATRINIK_FUZZ_${name}=1")
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer -Wno-unused-function)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    target_link_libraries(${target} PRIVATE atrinik-toolkit)
    atrinik_configure_target(${target})
endforeach ()

set(ATRINIK_FUZZ_RUNS 2000 CACHE STRING "Runs per target in the bounded fuzz smoke test")
set(fuzz_commands)
foreach (name IN LISTS ATRINIK_FUZZ_TARGETS)
    set(build_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/${name}")
    list(APPEND fuzz_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${build_corpus}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}"
            "${build_corpus}"
        COMMAND ${CMAKE_COMMAND} -E env
            ASAN_OPTIONS=detect_leaks=0
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
            $<TARGET_FILE:fuzz-${name}>
            -runs=${ATRINIK_FUZZ_RUNS}
            -max_len=65534
            "${build_corpus}")
endforeach ()

add_custom_target(fuzz-smoke
    ${fuzz_commands}
    DEPENDS ${ATRINIK_FUZZ_EXECUTABLES}
    USES_TERMINAL
    COMMENT "Running bounded protocol fuzz smoke tests")
