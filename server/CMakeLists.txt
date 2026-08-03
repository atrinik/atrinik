# The Atrinik server CMakeLists file.

cmake_minimum_required(VERSION 3.10)
project(atrinik-server C)
set(EXECUTABLE atrinik-server)
set(PACKAGE_NAME "Atrinik Server")

# Need Flex...
find_package(FLEX REQUIRED)

# libdl is required.
find_library(DL_LIBRARY dl)

if (NOT DL_LIBRARY AND NOT MINGW)
    message(FATAL_ERROR "Could not find dl library.")
endif ()

# Add our includes.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/src/include)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
include_directories(${CMAKE_BINARY_DIR}/common/toolkit)
include_directories(src/http_server)
include_directories(src/random_maps)
include_directories(src/tests)
include_directories(src/types/monster/include)
include_directories(src/types/include)
include_directories(src)

# Scan for lexers.
set(LEXERS
    src/loaders/map_header.l
    src/loaders/object.l
    src/loaders/random_map.l)
set(LEXERS_OUT)

# Add command to parse the lexer files with flex.
foreach (name ${LEXERS})
    string(REPLACE ".l" "" name ${name})
    set(src "${name}.l")
    set(dst "${name}.c")
    string(REPLACE "src/loaders/" "" name ${name})
    flex_target(${name} "${PROJECT_SOURCE_DIR}/${src}" "${PROJECT_SOURCE_DIR}/${dst}" COMPILE_FLAGS "-Cfa -i --prefix yy_${name}")
    get_property(out VARIABLE PROPERTY FLEX_${name}_OUTPUTS)
    set(LEXERS_OUT ${LEXERS_OUT} ${out})
endforeach (name)

# Add sources.
include(src/cmake.txt)

# Check is used for unit tests, but is optional.
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK_PKG QUIET IMPORTED_TARGET check)

if (NOT MINGW)
    pkg_check_modules(MINIUPNPC REQUIRED IMPORTED_TARGET miniupnpc)
endif ()

find_library(CHECK_LIBRARY check)

if (CHECK_PKG_FOUND OR CHECK_LIBRARY)
    set(HAVE_CHECK true)
else ()
    message(STATUS "Could not find check library, unit tests will be disabled.")
endif ()

# GD is used for world maker, but like unit tests, is also optional.
find_library(GD_LIBRARY gd)

if (GD_LIBRARY)
    set(HAVE_WORLD_MAKER true)
else ()
    message(STATUS "Could not find gd library, world maker module will be disabled.")
endif ()

# Find unit test source files if check is installed.
if (CHECK_LIBRARY)
    set(SOURCES_CHECK
        src/tests/check.c
        src/tests/bugs/cursed_treasures.c
        src/tests/unit/commands/object.c
        src/tests/unit/server/arch.c
        src/tests/unit/server/attack.c
        src/tests/unit/server/ban.c
        src/tests/unit/server/bank.c
        src/tests/unit/server/cache.c
        src/tests/unit/server/living.c
        src/tests/unit/server/object.c
        src/tests/unit/server/re_cmp.c
        src/tests/unit/server/rune.c
        src/tests/unit/server/shop.c
        src/tests/unit/toolkit/math.c
        src/tests/unit/toolkit/memory.c
        src/tests/unit/toolkit/packet.c
        src/tests/unit/toolkit/pbkdf2.c
        src/tests/unit/toolkit/shstr.c
        src/tests/unit/toolkit/socket_asset.c
        src/tests/unit/toolkit/string.c
        src/tests/unit/toolkit/stringbuffer.c
        src/tests/unit/types/light_apply.c
        src/tests/unit/types/sound_ambient.c)
endif ()

# Find the world maker files, if enabled.
if (HAVE_WORLD_MAKER)
    set(SOURCES_WORLD_MAKER
        src/modules/world_maker.c)
endif ()

# Compile the sources into an executable.
add_executable(${EXECUTABLE} ${SOURCES} ${LEXERS_OUT} ${SOURCES_CHECK} ${SOURCES_WORLD_MAKER})
target_link_libraries(${EXECUTABLE} atrinik-toolkit)
target_link_libraries(${EXECUTABLE}
    pcpnatpmp::pcpnatpmp)

if (MINGW)
    find_library(ATRINIK_MINIUPNPC_STATIC_LIBRARY miniupnpc)
    if (NOT ATRINIK_MINIUPNPC_STATIC_LIBRARY)
        message(FATAL_ERROR "Could not find the static MiniUPnPc library.")
    endif ()

    target_compile_definitions(${EXECUTABLE} PRIVATE MINIUPNP_STATICLIB)
    target_link_libraries(${EXECUTABLE}
        ${ATRINIK_MINIUPNPC_STATIC_LIBRARY}
        ws2_32
        iphlpapi)
else ()
    target_link_libraries(${EXECUTABLE} PkgConfig::MINIUPNPC)
endif ()

# Link libraries.
if (DL_LIBRARY)
    target_link_libraries(${EXECUTABLE} ${DL_LIBRARY})
endif ()

# Link check.
if (HAVE_CHECK)
    if (CHECK_PKG_FOUND)
        target_link_libraries(${EXECUTABLE} PkgConfig::CHECK_PKG)
    else ()
        target_link_libraries(${EXECUTABLE} ${CHECK_LIBRARY})

        if (UNIX AND NOT APPLE)
            target_link_libraries(${EXECUTABLE} rt)
        endif ()
    endif ()
endif ()

# Link GD.
if (GD_LIBRARY)
    target_link_libraries(${EXECUTABLE} ${GD_LIBRARY})

    if (MINGW)
        target_link_libraries(${EXECUTABLE} png)
    endif ()
endif ()

# Get all the include directories used to compile the server.
get_directory_property(INCLUDES INCLUDE_DIRECTORIES)

# Go through the include directories and construct a string.
foreach (var ${INCLUDES})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
    set(INCLUDES_STRING "${INCLUDES_STRING} -I${var}")
endforeach (var)

# Go through the source files and construct a string.
foreach (var ${SOURCES} ${LEXERS_OUT})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
    set(SOURCES_STRING "${SOURCES_STRING};${var}")
endforeach (var)

add_custom_target(check
    COMMAND ${EXECUTABLE} --unit --logger_filter_stdout=-info,-devel
        --port_mapping=off
    COMMAND ${EXECUTABLE} --plugin_unit --port_mapping=off
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Executing unit tests..."
    VERBATIM
)

if (HAVE_CHECK)
    # Go through the source files and construct a string.
    foreach (var ${SOURCES_CHECK})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
        set(SOURCES_CHECK_STRING "${SOURCES_CHECK_STRING};${var}")
    endforeach (var)

    # Create a 'proto' build target, which generates the proto.h file.
    add_custom_target(proto_unit
        cproto -ve -o src/tests/check_proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_CHECK_STRING}
        COMMAND cmake -E echo "#ifndef __CPROTO__" > src/tests/check_proto.h
        COMMAND sed -e "s/\"\" //g" < src/tests/check_proto.h.bak >> src/tests/check_proto.h
        COMMAND cmake -E echo "#endif" >> src/tests/check_proto.h
        COMMAND cmake -E remove -f src/tests/check_proto.h.bak
        COMMAND python src/toolkit/adjust_proto.py src/tests/check_proto.h || exit 5
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating unit test prototypes..."
        VERBATIM)
endif ()

# Create the Arena plugin.
set(SOURCES_PLUGIN_ARENA
    src/plugins/plugin_arena/plugin_arena.c)
add_library(plugin_arena MODULE ${SOURCES_PLUGIN_ARENA})

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
        else ()
            # Older CMake versions do not provide FindPython.
            find_package(PythonLibs)
        endif ()
    endif ()

    if (PYTHONLIBS_FOUND)
        include_directories(${PYTHON_INCLUDE_DIRS})
        include_directories(src/plugins/plugin_python/include)
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
        target_link_libraries(plugin_python ${PYTHON_LIBRARIES})
        add_dependencies(${EXECUTABLE} plugin_python)
    else ()
        message(FATAL_ERROR "Python development files are required for the Python plugin.")
    endif ()
else ()
    message(STATUS "Python plugin disabled by ENABLE_PYTHON_PLUGIN=false.")
endif ()

add_dependencies(check ${EXECUTABLE} plugin_arena)
if (TARGET plugin_python)
    add_dependencies(check plugin_python)
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

# Configure the .h file with the configuration options (size of long,
# package version, etc).
configure_file(src/include/cmake.h.def src/include/cmake.h)
configure_file(src/include/version.h.def src/include/version.h)
