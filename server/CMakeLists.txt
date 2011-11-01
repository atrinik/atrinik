# The Atrinik server CMakeLists file.

project(atrinik-server C)
cmake_minimum_required(VERSION 2.6)

# Include build configuration.
include(build.config)

if (EXISTS build.user)
	include(build.user)
endif (EXISTS build.user)

# Compiling on GNU/Linux.
set(LINUX true)
# Plugin suffix.
set(PLUGIN_SUFFIX ".so")

# cURL is required by the metaserver.
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
set(HAVE_CURL true)

# zlib is required by the server.
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
set(HAVE_ZLIB true)

# Need Flex...
find_package(FLEX REQUIRED)

# libdl is required.
find_library(DL_LIBRARY dl)

if (NOT DL_LIBRARY)
	message(FATAL_ERROR "Could not find dl library.")
endif (NOT DL_LIBRARY)

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

# Find some useful executables.
find_program(COMPRESS compress)
find_program(UNCOMPRESS uncompress)
find_program(GZIP gzip)
find_program(GUNZIP gunzip)
find_program(BZIP bzip2)
find_program(BUNZIP bunzip2)

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

# Add some useful compile flags.
add_definitions(-Wall -Wextra)
add_definitions(${CUSTOM_WARNINGS})

if (ENABLE_EXTRA_WARNINGS)
	add_definitions(-ftree-vrp -Wdeclaration-after-statement -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wlogical-op -Waggregate-return -Wnested-externs -Wunreachable-code -Wstack-protector -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE -fbounds-check -ftrapv -fstack-protector-all -Wformat=2 -Wformat-security -Winit-self -Wshadow -Wunsafe-loop-optimizations -Wbad-function-cast)
endif (ENABLE_EXTRA_WARNINGS)

if (ENABLE_AGGRESSIVE_WARNINGS)
	add_definitions(-Wcast-qual -Wmissing-declarations)
endif (ENABLE_AGGRESSIVE_WARNINGS)

# Turn warnings into errors?
if (NOT DISABLE_WARNING_ERRORS)
	add_definitions(-Werror)
endif (NOT DISABLE_WARNING_ERRORS)

# Disable optimization?
if (DISABLE_OPTIMIZING)
	add_definitions(-O0)
endif (DISABLE_OPTIMIZING)

if (ENABLE_DEBUG)
	add_definitions(-g)
endif (ENABLE_DEBUG)

# Check sizes of 'long' and 'long long' to determine architecture (32-bit
# or 64-bit)
include(CheckTypeSize)
check_type_size("long" SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)

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

# Add sources.
set(SOURCES
	src/commands/chat.c
	src/commands/commands.c
	src/commands/misc.c
	src/commands/move.c
	src/commands/new.c
	src/commands/object.c
	src/commands/range.c
	src/commands/wiz.c
	src/random_maps/decor.c
	src/random_maps/door.c
	src/random_maps/exit.c
	src/random_maps/expand2x.c
	src/random_maps/floor.c
	src/random_maps/maze_gen.c
	src/random_maps/monster.c
	src/random_maps/random_map.c
	src/random_maps/rogue_layout.c
	src/random_maps/room_gen_onion.c
	src/random_maps/room_gen_spiral.c
	src/random_maps/snake.c
	src/random_maps/square_spiral.c
	src/random_maps/style.c
	src/random_maps/wall.c
	src/server/anim.c
	src/server/apply.c
	src/server/arch.c
	src/server/attack.c
	src/server/ban.c
	src/server/button.c
	src/server/cache.c
	src/server/daemon.c
	src/server/exp.c
	src/server/gods.c
	src/server/hiscore.c
	src/server/holy.c
	src/server/image.c
	src/server/info.c
	src/server/init.c
	src/server/item.c
	src/server/light.c
	src/server/links.c
	src/server/living.c
	src/server/logger.c
	src/server/login.c
	src/server/los.c
	src/server/main.c
	src/server/map.c
	src/server/mempool.c
	src/server/move.c
	src/server/object.c
	src/server/object_methods.c
	src/server/party.c
	src/server/pathfinder.c
	src/server/plugins.c
	src/server/porting.c
	src/server/quest.c
	src/server/race.c
	src/server/readable.c
	src/server/re-cmp.c
	src/server/region.c
	src/server/rune.c
	src/server/shop.c
	src/server/shstr.c
	src/server/skills.c
	src/server/skill_util.c
	src/server/spell_effect.c
	src/server/spell_util.c
	src/server/statistics.c
	src/server/stringbuffer.c
	src/server/swap.c
	src/server/time.c
	src/server/treasure.c
	src/server/utils.c
	src/server/weather.c
	src/skills/construction.c
	src/skills/inscription.c
	src/socket/image.c
	src/socket/info.c
	src/socket/init.c
	src/socket/item.c
	src/socket/loop.c
	src/socket/lowlevel.c
	src/socket/metaserver.c
	src/socket/request.c
	src/socket/sounds.c
	src/socket/updates.c
	src/types/common/apply.c
	src/types/common/describe.c
	src/types/common/process.c
	src/types/altar.c
	src/types/arrow.c
	src/types/beacon.c
	src/types/book.c
	src/types/bullet.c
	src/types/cone.c
	src/types/container.c
	src/types/creator.c
	src/types/detector.c
	src/types/disease.c
	src/types/door.c
	src/types/duplicator.c
	src/types/firewall.c
	src/types/food.c
	src/types/gate.c
	src/types/gravestone.c
	src/types/light.c
	src/types/lightning.c
	src/types/magic_mirror.c
	src/types/map_info.c
	src/types/marker.c
	src/types/monster.c
	src/types/pit.c
	src/types/player.c
	src/types/player_mover.c
	src/types/poison.c
	src/types/potion.c
	src/types/power_crystal.c
	src/types/rod.c
	src/types/savebed.c
	src/types/scroll.c
	src/types/shop_mat.c
	src/types/sign.c
	src/types/sound_ambient.c
	src/types/spawn_point.c
	src/types/swarm_spell.c
	src/types/teleporter.c
	src/types/waypoint.c)

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
		src/tests/unit/server/check_re_cmp.c
		src/tests/unit/server/check_shstr.c
		src/tests/unit/server/check_utils.c)
elseif (CHECK_LIBRARY)
	set(SOURCES_CHECK)
endif (CHECK_LIBRARY)

# Find the world maker files, if enabled.
if (HAVE_WORLD_MAKER)
	set(SOURCES_WORLD_MAKER
		src/modules/world_maker.c)
elseif (HAVE_WORLD_MAKER)
	set(SOURCES_WORLD_MAKER)
endif (HAVE_WORLD_MAKER)

# Compile the sources into an executable.
add_executable(atrinik-server ${SOURCES} ${LEXERS_OUT} ${SOURCES_CHECK} ${SOURCES_WORLD_MAKER})

# Link libraries.
target_link_libraries(atrinik-server ${CURL_LIBRARY})
target_link_libraries(atrinik-server ${ZLIB_LIBRARY})
target_link_libraries(atrinik-server ${DL_LIBRARY})

# Link math...
if (MATH_LIBRARY)
	target_link_libraries(atrinik-server ${MATH_LIBRARY})
endif (MATH_LIBRARY)

# Link crypt.
if (CRYPT_LIBRARY)
	target_link_libraries(atrinik-server ${CRYPT_LIBRARY})
endif (CRYPT_LIBRARY)

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
foreach (var ${SOURCES} ${LEXERS_OUT})
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
	set(SOURCES_STRING "${SOURCES_STRING} "${var}"")
endforeach (var)

# Create a 'proto' build target, which generates the proto.h file.
add_custom_target(proto
	cproto -ve -o src/include/proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_STRING}
	COMMAND cmake -E echo "#ifndef __CPROTO__" > src/include/proto.h
	COMMAND sed -e "/YY_BUFFER/d" -e "/yy_size_t/d" -e "s/\"\" //g" < src/include/proto.h.bak >> src/include/proto.h
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
		cproto -ve -o src/tests/check_proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_CHECK_STRING}
		COMMAND cmake -E echo "#ifndef __CPROTO__" > src/tests/check_proto.h
		COMMAND sed -e "s/\"\" //g" < src/tests/check_proto.h.bak >> src/tests/check_proto.h
		COMMAND cmake -E echo "#endif" >> src/tests/check_proto.h
		COMMAND cmake -E remove -f src/tests/check_proto.h.bak
		COMMAND python tools/adjust_proto.py src/tests/check_proto.h || exit 5
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Generating unit test prototypes..."
		VERBATIM)
endif (CHECK_LIBRARY)

# Create the Arena plugin.
set(SOURCES_PLUGIN_ARENA
	src/plugins/plugin_arena/plugin_arena.c)
add_library(plugin_arena MODULE ${SOURCES_PLUGIN_ARENA})

# Set the preferred order of Python versions to find.
set(Python_ADDITIONAL_VERSIONS 3.2 3.1 3.0 2.7 2.6)
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
