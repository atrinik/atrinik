# The Atrinik CMakeLists file.

project(atrinik C)
cmake_minimum_required(VERSION 2.6)
set(EXECUTABLE atrinik)

# Include build configuration.
include(build.config)

if (EXISTS build.user)
	include(build.user)
endif (EXISTS build.user)

# SDL is required to run the client.
find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})
set(HAVE_SDL true)

# SDL_image is also required, otherwise no bitmaps or map images could be
# displayed.
find_package(SDL_image REQUIRED)
include_directories(${SDLIMAGE_INCLUDE_DIR})
set(HAVE_SDL_IMAGE true)

# SDL_ttf is used for all text drawing, so it's required.
find_package(SDL_ttf REQUIRED)
include_directories(${SDLTTF_INCLUDE_DIR})
set(HAVE_SDL_TTF true)

# SDL_mixer is optional, as sound is not a must-have.
find_package(SDL_mixer)

if (SDLMIXER_FOUND)
	include_directories(${SDLMIXER_INCLUDE_DIR})
	set(HAVE_SDL_MIXER true)
else (SDLMIXER_FOUND)
	message(STATUS "Warning: SDL_mixer not found - sound support will be disabled")
endif (SDLMIXER_FOUND)

# X11 is optional, since it's only used for clipboard support on GNU/Linux.
# Thus, if X11 is not available, clipboard support will be disabled.
find_package(X11)

if (X11_FOUND)
	include_directories(${X11_INCLUDE_DIR})
	set(HAVE_X11 true)

	# Try to find X11 XMU.
	find_library(X11_XMU_LIBRARY Xmu /usr/openwin/lib)

	if (X11_XMU_LIBRARY)
		set(HAVE_X11_XMU true)
	else (X11_XMU_LIBRARY)
		message(STATUS "Warning: X11 XMU not found - ctrl+v support will be disabled")
	endif (X11_XMU_LIBRARY)
else (X11_FOUND)
	message(STATUS "Warning: X11 not found - clipboard support will be disabled")
endif (X11_FOUND)

# Add some useful compile flags.
add_definitions(-Wall -Wextra -Wno-unused-parameter)
add_definitions(${CUSTOM_WARNINGS})

if (ENABLE_EXTRA_WARNINGS)
	add_definitions(-ftree-vrp -Wdeclaration-after-statement -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-align -Wlogical-op -Waggregate-return -Wnested-externs -Wunreachable-code -Wstack-protector -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE -fbounds-check -ftrapv -fstack-protector-all -Wformat -Wformat-security -Winit-self -Wshadow -Wunsafe-loop-optimizations -Wbad-function-cast)
endif (ENABLE_EXTRA_WARNINGS)

if (ENABLE_AGGRESSIVE_WARNINGS)
	add_definitions(-Wcast-qual -Wmissing-declarations -Wformat=2)
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

# Add our includes.
include_directories(src/include)
include(src/toolkit/cmake-pre.txt)

# Add sources.
set(SOURCES
	src/client/animations.c
	src/client/client.c
	src/client/cmd_aliases.c
	src/client/commands.c
	src/client/curl.c
	src/client/ignore.c
	src/client/image.c
	src/client/item.c
	src/client/keybind.c
	src/client/main.c
	src/client/menu.c
	src/client/metaserver.c
	src/client/misc.c
	src/client/player.c
	src/client/server_files.c
	src/client/server_settings.c
	src/client/settings.c
	src/client/socket.c
	src/client/sound.c
	src/client/sprite.c
	src/client/texture.c
	src/client/tilestretcher.c
	src/client/updates.c
	src/client/upgrader.c
	src/client/video.c
	src/client/wrapper.c
	src/events/event.c
	src/events/keys.c
	src/events/move.c
	src/gui/book.c
	src/gui/characters.c
	src/gui/effects.c
	src/gui/fps.c
	src/gui/game_news.c
	src/gui/help.c
	src/gui/input.c
	src/gui/interface.c
	src/gui/inventory.c
	src/gui/login.c
	src/gui/main.c
	src/gui/map.c
	src/gui/menu_buttons.c
	src/gui/mplayer.c
	src/gui/notification.c
	src/gui/party.c
	src/gui/protections.c
	src/gui/quickslots.c
	src/gui/range.c
	src/gui/region_map.c
	src/gui/settings.c
	src/gui/settings_client.c
	src/gui/settings_keybinding.c
	src/gui/skills.c
	src/gui/spells.c
	src/gui/textwin.c
	src/gui/updater.c
	${SOURCES_TOOLKIT}
	src/toolkit_gui/button.c
	src/toolkit_gui/clipboard.c
	src/toolkit_gui/list.c
	src/toolkit_gui/popup.c
	src/toolkit_gui/progress.c
	src/toolkit_gui/range_buttons.c
	src/toolkit_gui/scrollbar.c
	src/toolkit_gui/SDL_gfx.c
	src/toolkit_gui/SDL_rotozoom.c
	src/toolkit_gui/text.c
	src/toolkit_gui/text_input.c
	src/toolkit_gui/tooltip.c
	src/toolkit_gui/widget.c)

# Compile the sources into an executable.
add_executable(${EXECUTABLE} ${SOURCES})
include(src/toolkit/cmake-post.txt)

# Link all the available libraries.
target_link_libraries(${EXECUTABLE} ${SDL_LIBRARY})
target_link_libraries(${EXECUTABLE} ${SDLIMAGE_LIBRARY})
target_link_libraries(${EXECUTABLE} ${SDLTTF_LIBRARY})

# Link SDL_mixer if available.
if (HAVE_SDL_MIXER)
	target_link_libraries(${EXECUTABLE} ${SDLMIXER_LIBRARY})
endif (HAVE_SDL_MIXER)

# Link X11 if available.
if (HAVE_X11)
	target_link_libraries(${EXECUTABLE} ${X11_LIBRARY})
endif (HAVE_X11)

# Link X11 XMU if available.
if (HAVE_X11_XMU)
	target_link_libraries(${EXECUTABLE} ${X11_XMU_LIBRARY})
endif (HAVE_X11_XMU)

# Get all the include directories used to compile the client.
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
	cproto -ve -o src/include/proto.h.bak -D__CPROTO__${INCLUDES_STRING}${SOURCES_STRING}
	COMMAND cmake -E echo "#ifndef __CPROTO__" > src/include/proto.h
	COMMAND sed -e "s/#if __STDC__/#ifdef __STDC__/" -e "s/__signed/signed/g" -e "/__inline/d" -e "s/\"\" //g" < src/include/proto.h.bak >> src/include/proto.h
	COMMAND cmake -E echo "#endif" >> src/include/proto.h
	COMMAND cmake -E remove -f src/include/proto.h.bak
	COMMAND python src/toolkit/adjust_proto.py src/include/proto.h || exit 5
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMENT "Generating prototypes..."
	VERBATIM)

# Installer.
if (WIN32 AND NOT UNIX)
	set(INSTALL_SUBDIR_BIN ".")
	set(INSTALL_SUBDIR_SHARE ".")
	set(INSTALL_SUBDIR_DOC ".")

	file(GLOB DLLS "${CMAKE_CURRENT_SOURCE_DIR}/*.dll")
	install(FILES ${DLLS} DESTINATION ${INSTALL_SUBDIR_BIN})
else (WIN32 AND NOT UNIX)
	set(INSTALL_SUBDIR_BIN "games")
	set(INSTALL_SUBDIR_SHARE "share/games/atrinik")
	set(INSTALL_SUBDIR_DOC "share/doc/atrinik")

	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/atrinik.desktop DESTINATION "share/applications")
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/bitmaps/icon.png DESTINATION "share/pixmaps" RENAME "atrinik.png")
endif (WIN32 AND NOT UNIX)

install(TARGETS atrinik DESTINATION ${INSTALL_SUBDIR_BIN})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/INSTALL ${CMAKE_CURRENT_SOURCE_DIR}/README ${CMAKE_CURRENT_SOURCE_DIR}/COPYING DESTINATION ${INSTALL_SUBDIR_DOC})
install(DIRECTORY bitmaps cache data fonts gfx_user media settings sfx srv_files DESTINATION ${INSTALL_SUBDIR_SHARE})

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fantasy multiplayer online role-playing game")
set(CPACK_PACKAGE_VENDOR "Atrinik Development Team")
set(CPACK_PACKAGE_CONTACT "Atrinik Development Team <atrinik-dev@lists.launchpad.net>")
set(CPACK_DEBIAN_PACKAGE_NAME "atrinik-client")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.5), libgcc1 (>= 1:4.1), libsdl-image1.2 (>= 1.2.5), libsdl1.2debian (>= 1.2.10-1), libcurl3 (>= 7.16), libsdl-mixer1.2 (>= 1), libsdl-ttf2.0-0 (>= 1), timidity (>= 1), zlib1g (>= 1), libx11-6, libxmu6")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Fantasy multiplayer online role-playing game with 2.5D graphics\n .\n Homepage: http://www.atrinik.org/")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_RPM_PACKAGE_NAME "atrinik-client")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Fantasy multiplayer online role-playing game with 2.5D graphics\n .\n Homepage: http://www.atrinik.org/")
set(CPACK_RPM_PACKAGE_LICENSE "GNU General Public License (GPL)")
set(CPACK_RPM_PACKAGE_GROUP "Games")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Atrinik ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_INSTALL_DIRECTORIES "media")
set(CPACK_PACKAGE_EXECUTABLES "atrinik" "Atrinik ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_NSIS_INSTALLED_ICON_NAME "atrinik")
set(CPACK_NSIS_DISPLAY_NAME "Atrinik ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

if (${PACKAGE_TYPE} STREQUAL "deb")
	set(CPACK_BINARY_DEB "ON")
elseif (${PACKAGE_TYPE} STREQUAL "nsis")
	set(CPACK_BINARY_NSIS "ON")
elseif (${PACKAGE_TYPE} STREQUAL "rpm")
	set(CPACK_BINARY_RPM "ON")
endif (${PACKAGE_TYPE} STREQUAL "deb")

set(CPACK_BINARY_STGZ "OFF")
set(CPACK_BINARY_TGZ "OFF")
set(CPACK_BINARY_TZ "OFF")
include(CPack)

# Configure the .h file with the configuration options (size of long,
# package version, etc).
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/include/cmake.h.def ${CMAKE_CURRENT_SOURCE_DIR}/src/include/cmake.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/include/version.h.def ${CMAKE_CURRENT_SOURCE_DIR}/src/include/version.h)
