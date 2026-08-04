if (NOT DEFINED GIT_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR
        NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "Missing idempotent patch input")
endif ()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --unidiff-zero
        --whitespace=nowarn "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE apply_check_result
    OUTPUT_QUIET
    ERROR_QUIET)

if (apply_check_result EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --unidiff-zero
            --whitespace=nowarn "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE apply_result)
    if (NOT apply_result EQUAL 0)
        message(FATAL_ERROR "Could not apply ${PATCH_FILE}")
    endif ()
    return()
endif ()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --unidiff-zero
        --whitespace=nowarn "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE reverse_check_result
    OUTPUT_QUIET
    ERROR_QUIET)
if (NOT reverse_check_result EQUAL 0)
    message(FATAL_ERROR
        "${PATCH_FILE} can neither be applied nor identified as already applied")
endif ()

message(STATUS "Patch already applied: ${PATCH_FILE}")
