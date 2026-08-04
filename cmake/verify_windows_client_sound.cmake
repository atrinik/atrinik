if (NOT DEFINED ATRINIK_OBJDUMP OR ATRINIK_OBJDUMP STREQUAL "")
    message(FATAL_ERROR "ATRINIK_OBJDUMP is required")
endif ()

if (NOT DEFINED ATRINIK_CLIENT_EXECUTABLE OR
        NOT EXISTS "${ATRINIK_CLIENT_EXECUTABLE}")
    message(FATAL_ERROR
        "Windows client executable does not exist: ${ATRINIK_CLIENT_EXECUTABLE}")
endif ()

execute_process(
    COMMAND "${ATRINIK_OBJDUMP}" -p "${ATRINIK_CLIENT_EXECUTABLE}"
    RESULT_VARIABLE objdump_result
    OUTPUT_VARIABLE objdump_output
    ERROR_VARIABLE objdump_error)

if (NOT "${objdump_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Could not inspect Windows client imports: ${objdump_error}")
endif ()

if (NOT objdump_output MATCHES
        "DLL Name:[ \t]+[Ss][Dd][Ll]_[Mm][Ii][Xx][Ee][Rr]\\.[Dd][Ll][Ll]")
    message(FATAL_ERROR
        "Windows client was built without an SDL_mixer runtime import")
endif ()

message(STATUS "Windows client sound import verified")
