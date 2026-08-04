if (NOT DEFINED ATRINIK_SOURCE_DIR OR NOT DEFINED ATRINIK_BINARY_DIR OR
        NOT DEFINED ATRINIK_RUNTIME_DIR OR
        NOT DEFINED ATRINIK_ARENA_PLUGIN OR
        NOT DEFINED Python3_EXECUTABLE)
    message(FATAL_ERROR "Missing server test runtime preparation input")
endif ()

cmake_path(ABSOLUTE_PATH ATRINIK_RUNTIME_DIR NORMALIZE
    OUTPUT_VARIABLE normalized_runtime_dir)
set(expected_runtime_dir "${ATRINIK_BINARY_DIR}/server-test-runtime")
cmake_path(ABSOLUTE_PATH expected_runtime_dir NORMALIZE
    OUTPUT_VARIABLE normalized_expected_runtime_dir)
if (NOT normalized_runtime_dir STREQUAL normalized_expected_runtime_dir)
    message(FATAL_ERROR
        "Refusing to replace unexpected server test runtime: ${normalized_runtime_dir}")
endif ()

set(ATRINIK_RUNTIME_DIR "${normalized_runtime_dir}")
set(runtime_server "${ATRINIK_RUNTIME_DIR}/server")
file(REMOVE_RECURSE "${ATRINIK_RUNTIME_DIR}")
file(MAKE_DIRECTORY
    "${runtime_server}"
    "${runtime_server}/data/tmp"
    "${runtime_server}/lib")

file(COPY "${ATRINIK_SOURCE_DIR}/server/install_data/"
    DESTINATION "${runtime_server}/data")
file(COPY
    "${ATRINIK_SOURCE_DIR}/server/ca-bundle.crt"
    "${ATRINIK_SOURCE_DIR}/server/permissions.cfg"
    "${ATRINIK_SOURCE_DIR}/server/server.cfg"
    DESTINATION "${runtime_server}")
file(COPY
    "${ATRINIK_ARENA_PLUGIN}"
    DESTINATION "${runtime_server}")
if (DEFINED ATRINIK_PYTHON_PLUGIN)
    file(COPY "${ATRINIK_PYTHON_PLUGIN}" DESTINATION "${runtime_server}")
endif ()

file(CREATE_LINK
    "${ATRINIK_SOURCE_DIR}/maps"
    "${ATRINIK_RUNTIME_DIR}/maps"
    SYMBOLIC)
file(CREATE_LINK
    "${ATRINIK_SOURCE_DIR}/server/resources"
    "${runtime_server}/resources"
    SYMBOLIC)

execute_process(
    COMMAND "${Python3_EXECUTABLE}"
        "${ATRINIK_SOURCE_DIR}/tools/collect.py"
        --dir "${ATRINIK_SOURCE_DIR}"
        --out "${runtime_server}/lib"
    RESULT_VARIABLE collect_result)
if (NOT collect_result EQUAL 0)
    message(FATAL_ERROR
        "Server resource collection failed with status ${collect_result}")
endif ()

file(TOUCH "${ATRINIK_RUNTIME_DIR}/.prepared")
