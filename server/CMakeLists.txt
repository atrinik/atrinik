# The Atrinik server CMakeLists file.

project(atrinik-server C)
cmake_minimum_required(VERSION 2.6)

# Include build configuration.
include(build.config)

# Compiling on GNU/Linux.
set(LINUX true)

# cURL is required by the metaserver.
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
set(HAVE_CURL true)

# zlib is required by the server.
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
set(HAVE_ZLIB true)

# Try to find some GNU/Linux libraries.
if (UNIX)
	find_library(MATH_LIBRARY m)
	find_library(CRYPT_LIBRARY crypt)

	if (NOT MATH_LIBRARY)
		message(FATAL_ERROR "Could not find math library.")
	endif (NOT MATH_LIBRARY)

	if (CRYPT_LIBRARY)
		set(HAVE_CRYPT true)
	elseif (CRYPT_LIBRARY)
		message(STATUS "Could not find crypt library, encryption of player passwords will be disabled.")
	endif (CRYPT_LIBRARY)
endif (UNIX)

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

# Try to use DES for encrypting player passwords, if possible.
find_library(DES_LIBRARY des)

if (DES_LIBRARY)
	set(HAVE_LIBDES true)
endif (DES_LIBRARY)

# Find some useful executables.
find_program(COMPRESS compress)
find_program(UNCOMPRESS uncompress)
find_program(GZIP gzip)
find_program(GUNZIP gunzip)
find_program(BZIP bzip2)
find_program(BUNZIP bunzip2)
find_program(FLEX_EXECUTABLE flex)

# Flex is required to compile, so if it was not found, bail out.
if (NOT FLEX_EXECUTABLE)
	message(FATAL_ERROR "Could not find flex executable.")
endif (NOT FLEX_EXECUTABLE)

include(CheckIncludeFiles)
include(CheckFunctionExists)

# Check for various include files.
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(time.h HAVE_TIME_H)
check_include_files(stddef.h HAVE_STDDEF_H)
check_include_files(dirent.h HAVE_DIRENT_H)
check_include_files(sys/ndir.h HAVE_SYS_NDIR_H)
check_include_files(sys/dir.h HAVE_SYS_DIR_H)
check_include_files(ndir.h HAVE_NDIR_H)
check_include_files(crypt.h HAVE_CRYPT_H)
check_include_files(arpa/inet.h HAVE_ARPA_INET_H)
check_include_files(des.h HAVE_DES_H)

# Check for various functions.
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strncasecmp HAVE_STRNCASECMP)
check_function_exists(stricmp HAVE_STRICMP)
check_function_exists(strnicmp HAVE_STRNICMP)
check_function_exists(srandom HAVE_SRANDOM)
check_function_exists(srand48 HAVE_SRAND48)
check_function_exists(srand HAVE_SRAND)
check_function_exists(getdtablesize HAVE_GETDTABLESIZE)
check_function_exists(sysconf HAVE_SYSCONF)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strerror HAVE_STRERROR)

# Add some useful compile flags.
add_definitions(-Wall -Wextra)

# Turn warnings into errors?
if (NOT DISABLE_WARNING_ERRORS)
	add_definitions(-Werror)
endif (NOT DISABLE_WARNING_ERRORS)

# Disable optimization?
if (DISABLE_OPTIMIZING)
	add_definitions(-O0)
endif (DISABLE_OPTIMIZING)

# Check sizes of 'long' and 'long long' to determine architecture (32-bit
# or 64-bit)
include(CheckTypeSize)
check_type_size("long" SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)

# Configure the .h file with the configuration options (size of long,
# package version, etc).
configure_file(src/include/cmake.h.def src/include/cmake.h)

# Add our includes.
include_directories(src/include)
include_directories(src/random_maps)
include_directories(src/tests)

# Scan for lexers.
file(GLOB LEXERS src/loaders/*.l)
set(LEX_FILES)

# Add command to parse the lexer files with flex.
foreach (name ${lexer_srcs})
	string(REPLACE ".l" "" name ${name})
	set(src "${CMAKE_CURRENT_SOURCE_DIR}/${name}.l")
	set(dst "${CMAKE_CURRENT_SOURCE_DIR}/${name}.c")
	add_custom_command(
		OUTPUT "${dst}"
		DEPENDS "${src}"
		COMMAND
		${FLEX_EXECUTABLE}
		-i -t --prefix yy_${name} -o${dst} ${src})
	set(LEX_FILES ${LEX_FILES} "${dst}")
endforeach (name)

# Add the lexer files target.
add_custom_target(RerunLex DEPENDS ${LEX_FILES})

# Add sources.
file(GLOB SOURCES src/commands/*.c src/loaders/*.c src/random_maps/*.c src/server/*.c src/skills/*.c src/socket/*.c src/types/*.c)

# Find unit test source files if check is installed.
if (CHECK_LIBRARY)
	file(GLOB SOURCES_CHECK src/tests/*.c src/tests/bugs/*.c src/tests/unit/commands/*.c src/tests/unit/server/*.c)
elseif (CHECK_LIBRARY)
	set(SOURCES_CHECK)
endif (CHECK_LIBRARY)

# Find the world maker files, if enabled.
if (HAVE_WORLD_MAKER)
	file(GLOB SOURCES_WORLD_MAKER src/modules/world_maker.c)
elseif (HAVE_WORLD_MAKER)
	set(SOURCES_WORLD_MAKER)
endif (HAVE_WORLD_MAKER)

# Compile the sources into an executable.
add_executable(atrinik-server ${SOURCES} ${LEX_FILES} ${SOURCES_CHECK} ${SOURCES_WORLD_MAKER})

# Link libraries.
target_link_libraries(atrinik-server ${CURL_LIBRARY})
target_link_libraries(atrinik-server ${ZLIB_LIBRARY})

# Link math...
if (MATH_LIBRARY)
	target_link_libraries(atrinik-server ${MATH_LIBRARY})
endif (MATH_LIBRARY)

# Link crypt.
if (CRYPT_LIBRARY)
	target_link_libraries(atrinik-server ${CRYPT_LIBRARY})
endif (CRYPT_LIBRARY)

# Link DES.
if (DES_LIBRARY)
	target_link_libraries(atrinik-server ${DES_LIBRARY})
endif (DES_LIBRARY)

# Link check.
if (CHECK_LIBRARY)
	target_link_libraries(atrinik-server ${CHECK_LIBRARY})
endif (CHECK_LIBRARY)

# Link GD.
if (GD_LIBRARY)
	target_link_libraries(atrinik-server ${GD_LIBRARY})
endif (GD_LIBRARY)

# Get all the include directories used to compile the server.
get_directory_property(INCLUDES INCLUDE_DIRECTORIES)

# Go through the include directories and construct a string.
foreach (var ${INCLUDES})
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
	set(INCLUDES_STRING "${INCLUDES_STRING} -I${var}")
endforeach (var)

# Go through the source files and construct a string.
foreach (var ${SOURCES})
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
	set(SOURCES_STRING "${SOURCES_STRING} "${var}"")
endforeach (var)

# Create a 'proto' build target, which generates the proto.h file.
add_custom_target(proto
	cproto -o src/include/proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_STRING}
	COMMAND cmake -E echo "#ifndef __CPROTO__" > src/include/proto.h
	COMMAND sed -e "/YY_BUFFER/d" -e "/yy_size_t/d" -e "s/(void);/();/g" -e "s/\"\" //g" < src/include/proto.h.bak >> src/include/proto.h
	COMMAND cmake -E echo "#endif" >> src/include/proto.h
	COMMAND cmake -E remove -f src/include/proto.h.bak
	COMMAND python tools/adjust_proto.py src/include/proto.h || exit 5
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMENT "Generating prototypes..."
	VERBATIM)

if (CHECK_LIBRARY)
	# Go through the source files and construct a string.
	foreach (var ${SOURCES_CHECK})
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
		set(SOURCES_CHECK_STRING "${SOURCES_CHECK_STRING} "${var}"")
	endforeach (var)

	# Create a 'proto' build target, which generates the proto.h file.
	add_custom_target(proto_unit
		cproto -o src/tests/check_proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_CHECK_STRING}
		COMMAND cmake -E echo "#ifndef __CPROTO__" > src/tests/check_proto.h
		COMMAND sed -e "s/(void);/();/g" -e "s/\"\" //g" < src/tests/check_proto.h.bak >> src/tests/check_proto.h
		COMMAND cmake -E echo "#endif" >> src/tests/check_proto.h
		COMMAND cmake -E remove -f src/tests/check_proto.h.bak
		COMMAND python tools/adjust_proto.py src/tests/check_proto.h || exit 5
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Generating unit test prototypes..."
		VERBATIM)
endif (CHECK_LIBRARY)

# Create the Arena plugin.
file(GLOB SOURCES_PLUGIN_ARENA src/plugins/plugin_arena/*.c)
add_library(plugin_arena MODULE ${SOURCES_PLUGIN_ARENA})

# Set the preferred order of Python versions to find.
set(Python_ADDITIONAL_VERSIONS 3.2 3.1 3.0 2.7 2.6)
# And try to find Python.
find_package(PythonLibs)

# Create the Python plugin, if Python was found.
if (PYTHONLIBS_FOUND)
	include_directories(${PYTHON_INCLUDE_DIRS})
	include_directories(src/plugins/plugin_python/include)
	set(HAVE_PYTHON true)

	add_definitions(-Wno-missing-field-initializers)
	file(GLOB SOURCES_PLUGIN_PYTHON src/plugins/plugin_python/*.c)
	add_library(plugin_python MODULE ${SOURCES_PLUGIN_PYTHON})
	target_link_libraries(plugin_python ${PYTHON_LIBRARIES})
elseif (PYTHONLIBS_FOUND)
	message(STATUS "Python libs not found, Python plugin will not be built.")
endif (PYTHONLIBS_FOUND)
