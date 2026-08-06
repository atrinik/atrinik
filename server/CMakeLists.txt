# The Atrinik server CMakeLists file.

set(EXECUTABLE atrinik-server)
set(PACKAGE_NAME "Atrinik Server")

set(ATRINIK_SERVER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_BINARY_DIR}/common
    ${CMAKE_BINARY_DIR}/common/toolkit
    ${CMAKE_CURRENT_BINARY_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http_server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random_maps
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src/types/monster/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(FLEX REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/loaders")
set(LEXERS_OUT)
foreach (name map_header object random_map)
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/src/loaders/${name}.l")
    set(dst "${CMAKE_CURRENT_BINARY_DIR}/src/loaders/${name}.c")
    flex_target(${name} "${src}" "${dst}"
        COMPILE_FLAGS "-Cfa -i --prefix yy_${name}")
    list(APPEND LEXERS_OUT ${FLEX_${name}_OUTPUTS})
endforeach ()

include(src/cmake.txt)
find_package(PkgConfig REQUIRED)

if (NOT MINGW)
    pkg_check_modules(MINIUPNPC REQUIRED IMPORTED_TARGET miniupnpc)
endif ()

find_library(DL_LIBRARY dl)
if (NOT DL_LIBRARY AND NOT MINGW)
    message(FATAL_ERROR "Could not find dl library.")
endif ()

find_library(GD_LIBRARY gd)
if (GD_LIBRARY)
    set(HAVE_WORLD_MAKER true)
    set(SOURCES_WORLD_MAKER src/modules/world_maker.c)
else ()
    message(STATUS "Could not find gd library, world maker module will be disabled.")
endif ()

configure_file(src/include/cmake.h.def src/include/cmake.h)
configure_file(src/include/version.h.def src/include/version.h)

add_library(atrinik-server-core OBJECT
    ${SERVER_CORE_SOURCES}
    ${SOURCES_WORLD_MAKER}
    ${LEXERS_OUT})
atrinik_configure_target(atrinik-server-core)
target_include_directories(atrinik-server-core BEFORE PRIVATE
    ${ATRINIK_SERVER_INCLUDE_DIRS})
target_link_libraries(atrinik-server-core PRIVATE
    atrinik-toolkit
    pcpnatpmp::pcpnatpmp)

if (MINGW)
    find_library(ATRINIK_MINIUPNPC_STATIC_LIBRARY miniupnpc)
    if (NOT ATRINIK_MINIUPNPC_STATIC_LIBRARY)
        message(FATAL_ERROR "Could not find the static MiniUPnPc library.")
    endif ()

    target_compile_definitions(atrinik-server-core PRIVATE MINIUPNP_STATICLIB)
    target_link_libraries(atrinik-server-core PRIVATE
        ${ATRINIK_MINIUPNPC_STATIC_LIBRARY}
        ws2_32
        iphlpapi)
else ()
    target_link_libraries(atrinik-server-core PRIVATE PkgConfig::MINIUPNPC)
endif ()

if (DL_LIBRARY)
    target_link_libraries(atrinik-server-core PRIVATE ${DL_LIBRARY})
endif ()

if (GD_LIBRARY)
    target_link_libraries(atrinik-server-core PRIVATE ${GD_LIBRARY})
    if (MINGW)
        target_link_libraries(atrinik-server-core PRIVATE png)
    endif ()
endif ()

add_executable(${EXECUTABLE} src/server/entrypoint.c)
atrinik_configure_target(${EXECUTABLE})
target_include_directories(${EXECUTABLE} BEFORE PRIVATE
    ${ATRINIK_SERVER_INCLUDE_DIRS})
target_link_libraries(${EXECUTABLE} PRIVATE atrinik-server-core)

add_library(plugin_arena MODULE
    src/plugins/plugin_arena/plugin_arena.c)
atrinik_configure_target(plugin_arena)
target_include_directories(plugin_arena BEFORE PRIVATE
    ${ATRINIK_SERVER_INCLUDE_DIRS})

# Create the Python plugin, if requested and Python was found.
if (ENABLE_PYTHON_PLUGIN)
    set(ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR "" CACHE PATH
        "Target Python headers for a MinGW server build")
    set(ATRINIK_WINDOWS_PYTHON_LIBRARY "" CACHE FILEPATH
        "Target Python import library for a MinGW server build")

    if (MINGW AND ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR AND
            ATRINIK_WINDOWS_PYTHON_LIBRARY)
        set(PYTHONLIBS_FOUND true)
        set(PYTHON_INCLUDE_DIRS ${ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR})
        set(PYTHON_LIBRARIES ${ATRINIK_WINDOWS_PYTHON_LIBRARY})
    else ()
        find_package(Python QUIET COMPONENTS Development)

        if (Python_Development_FOUND)
            set(PYTHONLIBS_FOUND true)
            set(PYTHON_INCLUDE_DIRS ${Python_INCLUDE_DIRS})
            set(PYTHON_LIBRARIES Python::Python)
        endif ()
    endif ()

    if (PYTHONLIBS_FOUND)
        set(HAVE_PYTHON true)

        set(SOURCES_PLUGIN_PYTHON
            src/plugins/plugin_python/atrinik_archetype.c
            src/plugins/plugin_python/atrinik_map.c
            src/plugins/plugin_python/atrinik_object.c
            src/plugins/plugin_python/atrinik_party.c
            src/plugins/plugin_python/atrinik_player.c
            src/plugins/plugin_python/atrinik_region.c
            src/plugins/plugin_python/attr_list.c
            src/plugins/plugin_python/plugin_python.c)
        add_library(plugin_python MODULE ${SOURCES_PLUGIN_PYTHON})
        atrinik_configure_target(plugin_python)
        target_include_directories(plugin_python BEFORE PRIVATE
            ${ATRINIK_SERVER_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/plugin_python/include
            ${PYTHON_INCLUDE_DIRS})
        target_link_libraries(plugin_python PRIVATE ${PYTHON_LIBRARIES})
        add_dependencies(${EXECUTABLE} plugin_python)
    else ()
        message(FATAL_ERROR "Python development files are required for the Python plugin.")
    endif ()
else ()
    message(STATUS "Python plugin disabled by ENABLE_PYTHON_PLUGIN=false.")
endif ()

if (BUILD_TESTING AND NOT MINGW)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    pkg_check_modules(CHECK_PKG QUIET IMPORTED_TARGET check)

    if (CHECK_PKG_FOUND)
        include(src/tests/suites.cmake)
        set(SOURCES_CHECK
            src/tests/main.c
            src/tests/check.c
            src/tests/server_clock_fake.c)
        set(ATRINIK_SERVER_UNIT_SUITES)
        set(ATRINIK_SERVER_TEST_SUITE_DECLARATIONS)
        set(ATRINIK_SERVER_TEST_SUITE_ENTRIES)
        foreach (suite_record IN LISTS ATRINIK_SERVER_TEST_SUITES)
            string(REPLACE "|" ";" suite_fields "${suite_record}")
            list(LENGTH suite_fields suite_field_count)
            if (NOT suite_field_count EQUAL 3)
                message(FATAL_ERROR "Invalid server test suite: ${suite_record}")
            endif ()

            list(GET suite_fields 0 suite_name)
            list(GET suite_fields 1 suite_function)
            list(GET suite_fields 2 suite_source)
            list(APPEND ATRINIK_SERVER_UNIT_SUITES "${suite_name}")
            list(APPEND SOURCES_CHECK "${suite_source}")
            string(APPEND ATRINIK_SERVER_TEST_SUITE_DECLARATIONS
                "extern void ${suite_function}(void);\n")
            string(APPEND ATRINIK_SERVER_TEST_SUITE_ENTRIES
                "    {\"${suite_name}\", ${suite_function}},\n")
        endforeach ()

        configure_file(
            src/tests/check_suites.h.in
            src/include/check_suites.h
            @ONLY)
        configure_file(
            src/tests/check_suites.inc.in
            src/include/check_suites.inc
            @ONLY)

        add_executable(atrinik-server-tests ${SOURCES_CHECK})
        atrinik_configure_target(atrinik-server-tests)
        target_include_directories(atrinik-server-tests BEFORE PRIVATE
            ${ATRINIK_SERVER_INCLUDE_DIRS})
        target_compile_definitions(atrinik-server-tests PRIVATE
            ATRINIK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/tests/data")
        target_link_libraries(atrinik-server-tests PRIVATE
            atrinik-server-core
            PkgConfig::CHECK_PKG)

        set(ATRINIK_SERVER_TEST_RUNTIME
            "${CMAKE_BINARY_DIR}/server-test-runtime")
        set(ATRINIK_SERVER_TEST_RUNTIME_STAMP
            "${ATRINIK_SERVER_TEST_RUNTIME}/.prepared")
        set(ATRINIK_SERVER_TEST_RUNTIME_ARGS
            -DATRINIK_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DATRINIK_BINARY_DIR=${CMAKE_BINARY_DIR}
            -DATRINIK_RUNTIME_DIR=${ATRINIK_SERVER_TEST_RUNTIME}
            -DATRINIK_ARENA_PLUGIN=$<TARGET_FILE:plugin_arena>
            -DPython3_EXECUTABLE=${Python3_EXECUTABLE})
        set(ATRINIK_SERVER_TEST_RUNTIME_DEPENDS plugin_arena)
        if (TARGET plugin_python)
            list(APPEND ATRINIK_SERVER_TEST_RUNTIME_ARGS
                -DATRINIK_PYTHON_PLUGIN=$<TARGET_FILE:plugin_python>)
            list(APPEND ATRINIK_SERVER_TEST_RUNTIME_DEPENDS plugin_python)
        endif ()

        file(GLOB_RECURSE ATRINIK_SERVER_TEST_RESOURCE_INPUTS
            CONFIGURE_DEPENDS
            LIST_DIRECTORIES false
            "${CMAKE_SOURCE_DIR}/arch/*"
            "${CMAKE_SOURCE_DIR}/server/install_data/*"
            "${CMAKE_SOURCE_DIR}/tools/compilers/*.py")
        list(APPEND ATRINIK_SERVER_TEST_RESOURCE_INPUTS
            ${CMAKE_SOURCE_DIR}/server/ca-bundle.crt
            ${CMAKE_SOURCE_DIR}/server/permissions.cfg
            ${CMAKE_SOURCE_DIR}/server/server.cfg
            ${CMAKE_SOURCE_DIR}/tools/collect.py
            ${CMAKE_SOURCE_DIR}/tools/utils.py)

        add_custom_command(
            OUTPUT "${ATRINIK_SERVER_TEST_RUNTIME_STAMP}"
            COMMAND ${CMAKE_COMMAND}
                ${ATRINIK_SERVER_TEST_RUNTIME_ARGS}
                -P ${CMAKE_SOURCE_DIR}/cmake/prepare_server_test_runtime.cmake
            DEPENDS
                ${ATRINIK_SERVER_TEST_RUNTIME_DEPENDS}
                ${ATRINIK_SERVER_TEST_RESOURCE_INPUTS}
                ${CMAKE_SOURCE_DIR}/cmake/prepare_server_test_runtime.cmake
            COMMENT "Preparing isolated server test runtime..."
            VERBATIM)
        add_custom_target(server-test-runtime
            DEPENDS "${ATRINIK_SERVER_TEST_RUNTIME_STAMP}")
        add_dependencies(atrinik-server-tests server-test-runtime)

        foreach (suite IN LISTS ATRINIK_SERVER_UNIT_SUITES)
            add_test(NAME server-unit-${suite}
                COMMAND atrinik-server-tests
                    --unit
                    --logger_filter_stdout=-info,-devel
                    --port_mapping=off)
            set_tests_properties(server-unit-${suite} PROPERTIES
                ENVIRONMENT "ATRINIK_TEST_SUITE=${suite}"
                LABELS "server;unit"
                RESOURCE_LOCK server-test-runtime
                TIMEOUT 300
                WORKING_DIRECTORY "${ATRINIK_SERVER_TEST_RUNTIME}/server")
        endforeach ()

        if (ENABLE_SANITIZERS)
            add_test(NAME server-lsan-smoke
                COMMAND atrinik-server-tests
                    --unit
                    --logger_filter_stdout=-info,-devel
                    --port_mapping=off)
            set_tests_properties(server-lsan-smoke PROPERTIES
                ENVIRONMENT
                    "ASAN_OPTIONS=detect_leaks=1;ATRINIK_TEST_SUITE=toolkit.string;CK_FORK=no"
                LABELS "server;unit;sanitizer;lsan"
                RESOURCE_LOCK server-test-runtime
                TIMEOUT 300
                WORKING_DIRECTORY "${ATRINIK_SERVER_TEST_RUNTIME}/server")
        endif ()

        if (TARGET plugin_python)
            add_test(NAME server-plugin-python
                COMMAND ${EXECUTABLE} --plugin_unit --port_mapping=off)
            set_tests_properties(server-plugin-python PROPERTIES
                LABELS "server;plugin;runtime"
                RESOURCE_LOCK server-test-runtime
                TIMEOUT 300
                WORKING_DIRECTORY "${ATRINIK_SERVER_TEST_RUNTIME}/server")
        endif ()

        add_custom_target(check
            COMMAND ${CMAKE_CTEST_COMMAND}
                --test-dir ${CMAKE_BINARY_DIR}
                --output-on-failure
                -L server
            DEPENDS
                atrinik-server-tests
                ${EXECUTABLE}
                server-test-runtime
            COMMENT "Executing server tests..."
            VERBATIM)
    else ()
        message(WARNING
            "Check was not found; dedicated server tests will not be built.")
    endif ()
endif ()

if (MINGW AND ${PACKAGE_TYPE} STREQUAL "zip")
    if (NOT TARGET plugin_python)
        message(FATAL_ERROR
            "The portable Windows server package requires the Python plugin.")
    endif ()

    set(ATRINIK_WINDOWS_RUNTIME_DIR "" CACHE PATH
        "Directory containing MinGW runtime DLLs")
    set(ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR "" CACHE PATH
        "Directory containing the embedded Windows Python runtime")
    set(ATRINIK_SERVER_PACKAGE_ROOT "" CACHE PATH
        "Prepared maps and collected resources for the server package")

    foreach (required_dir
            ATRINIK_WINDOWS_RUNTIME_DIR
            ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR
            ATRINIK_SERVER_PACKAGE_ROOT)
        if (NOT IS_DIRECTORY "${${required_dir}}")
            message(FATAL_ERROR
                "${required_dir} is not a directory: ${${required_dir}}")
        endif ()
    endforeach ()

    if (NOT IS_DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/maps" OR
            NOT IS_DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/lib" OR
            NOT IS_DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/client-maps")
        message(FATAL_ERROR
            "ATRINIK_SERVER_PACKAGE_ROOT must contain maps/, lib/, and "
            "client-maps/.")
    endif ()

    install(TARGETS ${EXECUTABLE} plugin_arena plugin_python
        RUNTIME DESTINATION server
        LIBRARY DESTINATION server)

    file(GLOB ATRINIK_WINDOWS_RUNTIME_DLLS
        "${ATRINIK_WINDOWS_RUNTIME_DIR}/*.dll")
    list(FILTER ATRINIK_WINDOWS_RUNTIME_DLLS EXCLUDE REGEX
        "/libffi-8\\.dll$")
    if (NOT ATRINIK_WINDOWS_RUNTIME_DLLS)
        message(FATAL_ERROR
            "No DLLs found in ATRINIK_WINDOWS_RUNTIME_DIR: ${ATRINIK_WINDOWS_RUNTIME_DIR}")
    endif ()
    install(FILES ${ATRINIK_WINDOWS_RUNTIME_DLLS} DESTINATION server)

    install(DIRECTORY "${ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR}/"
        DESTINATION server)
    install(DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/maps"
        DESTINATION .
        PATTERN ".git" EXCLUDE)
    install(DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/lib/"
        DESTINATION server/lib)
    install(DIRECTORY "${ATRINIK_SERVER_PACKAGE_ROOT}/client-maps"
        DESTINATION server/install_data/http)
    install(DIRECTORY install_data resources tools
        DESTINATION server
        PATTERN ".git" EXCLUDE)
    install(FILES
        README
        COPYING
        ca-bundle.crt
        permissions.cfg
        server.bat
        server.cfg
        ${CMAKE_SOURCE_DIR}/server-custom.cfg.example
        DESTINATION server)

    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Atrinik multiplayer online role-playing game server")
    set(CPACK_PACKAGE_VENDOR "Atrinik Development Team")
    set(CPACK_PACKAGE_CONTACT
        "Atrinik Development Team <admin@atrinik.org>")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PACKAGE_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PACKAGE_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PACKAGE_VERSION_PATCH}")
    set(CPACK_PACKAGE_FILE_NAME
        "atrinik-server-${PACKAGE_VERSION}-windows-x86_64")
    set(CPACK_BINARY_ZIP ON)
    set(CPACK_BINARY_STGZ OFF)
    set(CPACK_BINARY_TGZ OFF)
    set(CPACK_BINARY_TZ OFF)
    include(CPack)
endif ()
