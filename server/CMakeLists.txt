# The Atrinik server CMakeLists file.

project(atrinik-server C)
cmake_minimum_required(VERSION 2.6)
set(EXECUTABLE atrinik-server)

# Include build configuration.
include(build.config)

if (EXISTS build.user)
	include(build.user)
endif (EXISTS build.user)

# Need Flex...
find_package(FLEX REQUIRED)

# libpthread is required.
find_package(Threads REQUIRED)

# libdl is required.
find_library(DL_LIBRARY dl)

if (NOT DL_LIBRARY AND NOT MINGW)
	message(FATAL_ERROR "Could not find dl library.")
endif ()

# Check is used for unit tests, but is optional.
find_library(CHECK_LIBRARY check)

if (CHECK_LIBRARY)
	set(HAVE_CHECK true)
elseif (CHECK_LIBRARY)
	message(STATUS "Could not find check library, unit tests will be disabled.")
endif (CHECK_LIBRARY)

# GD is used for world maker, but like unit tests, is also optional.
find_library(GD_LIBRARY gd)

if (GD_LIBRARY)
	set(HAVE_WORLD_MAKER true)
elseif (GD_LIBRARY)
	message(STATUS "Could not find gd library, world maker module will be disabled.")
endif (GD_LIBRARY)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG=1 -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DDEBUG=0 -O3")

# Add some useful compile flags.
add_definitions(-Wall -Wextra -Wno-unused-parameter)
add_definitions(${CUSTOM_WARNINGS})

if (ENABLE_EXTRA_WARNINGS)
	add_definitions(-ftree-vrp -Wdeclaration-after-statement -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wlogical-op -Waggregate-return -Wnested-externs -Wunreachable-code -Wstack-protector -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE -fbounds-check -ftrapv -fstack-protector-all -Wformat=2 -Wformat-security -Winit-self -Wshadow -Wunsafe-loop-optimizations -Wbad-function-cast)
endif (ENABLE_EXTRA_WARNINGS)

if (ENABLE_AGGRESSIVE_WARNINGS)
	add_definitions(-Wcast-qual -Wmissing-declarations)
endif (ENABLE_AGGRESSIVE_WARNINGS)

# Turn warnings into errors?
if (ENABLE_WARNING_ERRORS)
	add_definitions(-Werror)
endif (ENABLE_WARNING_ERRORS)

# Configure the .h file with the configuration options (size of long,
# package version, etc).
configure_file(src/include/cmake.h.def src/include/cmake.h)
configure_file(src/include/version.h.def src/include/version.h)

# Add our includes.
include_directories(src/include)
include_directories(src/random_maps)
include_directories(src/tests)

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
	flex_target(${name} ${src} ${dst} COMPILE_FLAGS "-Cfa -i --prefix yy_${name}")
	get_property(out VARIABLE PROPERTY FLEX_${name}_OUTPUTS)
	set(LEXERS_OUT ${LEXERS_OUT} ${out})
endforeach (name)

include(src/toolkit/cmake-pre.txt)

# Add sources.
include(src/cmake.txt)

# Find unit test source files if check is installed.
if (CHECK_LIBRARY)
	set(SOURCES_CHECK
		src/tests/check.c
		src/tests/bugs/check_85.c
		src/tests/unit/commands/check_object.c
		src/tests/unit/server/check_arch.c
		src/tests/unit/server/check_ban.c
		src/tests/unit/server/check_cache.c
		src/tests/unit/server/check_object.c
        src/tests/unit/server/check_pbkdf2.c
		src/tests/unit/server/check_re_cmp.c
		src/tests/unit/server/check_shstr.c
        src/tests/unit/server/check_string.c
		src/tests/unit/server/check_utils.c)
endif (CHECK_LIBRARY)

# Find the world maker files, if enabled.
if (HAVE_WORLD_MAKER)
	set(SOURCES_WORLD_MAKER
		src/modules/world_maker.c)
endif (HAVE_WORLD_MAKER)

# Compile the sources into an executable.
add_executable(${EXECUTABLE} ${SOURCES} ${LEXERS_OUT} ${SOURCES_CHECK} ${SOURCES_WORLD_MAKER})
include(src/toolkit/cmake-post.txt)

# Link libraries.
if (DL_LIBRARY)
    target_link_libraries(${EXECUTABLE} ${DL_LIBRARY})
endif ()

# Link check.
if (CHECK_LIBRARY)
	if (UNIX AND NOT APPLE)
		target_link_libraries(${EXECUTABLE} rt)
	endif ()

	target_link_libraries(${EXECUTABLE} ${CHECK_LIBRARY})
endif (CHECK_LIBRARY)

# Link GD.
if (GD_LIBRARY)
	target_link_libraries(${EXECUTABLE} ${GD_LIBRARY})
endif (GD_LIBRARY)

target_link_libraries(${EXECUTABLE} ${CMAKE_THREAD_LIBS_INIT})

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

# Create a 'proto' build target, which generates the proto.h file.
add_custom_target(proto
	cproto -ve -o src/include/proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_STRING}
	COMMAND cmake -E echo "#ifndef __CPROTO__" > src/include/proto.h
	COMMAND sed -e "/YY_BUFFER/d" -e "/yy_size_t/d" -e "s/\"\" //g" < src/include/proto.h.bak >> src/include/proto.h
	COMMAND cmake -E echo "#endif" >> src/include/proto.h
	COMMAND cmake -E remove -f src/include/proto.h.bak
	COMMAND python src/toolkit/adjust_proto.py src/include/proto.h || exit 5
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMENT "Generating prototypes..."
	VERBATIM)

if (CHECK_LIBRARY)
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
endif (CHECK_LIBRARY)

# Create the Arena plugin.
set(SOURCES_PLUGIN_ARENA
	src/plugins/plugin_arena/plugin_arena.c)
add_library(plugin_arena MODULE ${SOURCES_PLUGIN_ARENA})

# Set the preferred order of Python versions to find.
set(Python_ADDITIONAL_VERSIONS 3.4 3.3 3.2 3.1 3.0 2.7)
set(CMAKE_USE_PYTHON_VERSION ${Python_ADDITIONAL_VERSIONS})
# And try to find Python.
find_package(PythonLibs)

# Create the Python plugin, if Python was found.
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
elseif (PYTHONLIBS_FOUND)
	message(STATUS "Python libs not found, Python plugin will not be built.")
endif (PYTHONLIBS_FOUND)
