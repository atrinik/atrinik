find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(ATRINIK_NETWORK_E2E_DRIVER atrinik-network-e2e-driver)
add_executable(${ATRINIK_NETWORK_E2E_DRIVER}
    network_driver.c
    ../../server/src/socket/port_mapping_controller.c)
target_include_directories(${ATRINIK_NETWORK_E2E_DRIVER} PRIVATE
    ${CMAKE_BINARY_DIR}/common/toolkit
    ../../server/src/include)
target_link_libraries(${ATRINIK_NETWORK_E2E_DRIVER} atrinik-toolkit)
set_target_properties(${ATRINIK_NETWORK_E2E_DRIVER} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/e2e")

add_custom_target(network-e2e
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/run.py"
        --driver "$<TARGET_FILE:${ATRINIK_NETWORK_E2E_DRIVER}>"
    DEPENDS ${ATRINIK_NETWORK_E2E_DRIVER}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Running isolated UDP/QUIC end-to-end tests..."
    VERBATIM)
