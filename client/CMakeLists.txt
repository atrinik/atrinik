# The Atrinik CMakeLists file.

cmake_minimum_required(VERSION 3.21)
project(atrinik C)
set(EXECUTABLE atrinik)
set(EXECUTABLE_UPDATER atrinik2)
set(PACKAGE_NAME "Atrinik Client")

# Add our includes.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/src/include)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
include_directories(${CMAKE_BINARY_DIR}/common/toolkit)
include_directories(src/gui/toolkit/include)
include_directories(src)

if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
endif ()

# SDL is required to run the client.
find_package(SDL REQUIRED)
if (NOT SDL_INCLUDE_DIR AND SDL_INCLUDE_DIRS)
    set(SDL_INCLUDE_DIR ${SDL_INCLUDE_DIRS})
endif ()
if (NOT SDL_LIBRARY AND SDL_LIBRARIES)
    set(SDL_LIBRARY ${SDL_LIBRARIES})
endif ()
include_directories(${SDL_INCLUDE_DIR})
set(HAVE_SDL true)

# SDL_image is also required, otherwise no bitmaps or map images could be
# displayed.
find_package(SDL_image REQUIRED)
if (NOT SDLIMAGE_INCLUDE_DIR AND SDL_IMAGE_INCLUDE_DIRS)
    set(SDLIMAGE_INCLUDE_DIR ${SDL_IMAGE_INCLUDE_DIRS})
endif ()
if (NOT SDLIMAGE_LIBRARY AND SDL_IMAGE_LIBRARIES)
    set(SDLIMAGE_LIBRARY ${SDL_IMAGE_LIBRARIES})
endif ()
include_directories(${SDLIMAGE_INCLUDE_DIR})
set(HAVE_SDL_IMAGE true)

# SDL_ttf is used for all text drawing, so it's required.
find_package(SDL_ttf REQUIRED)
if (NOT SDLTTF_INCLUDE_DIR AND SDL_TTF_INCLUDE_DIRS)
    set(SDLTTF_INCLUDE_DIR ${SDL_TTF_INCLUDE_DIRS})
endif ()
if (NOT SDLTTF_LIBRARY AND SDL_TTF_LIBRARIES)
    set(SDLTTF_LIBRARY ${SDL_TTF_LIBRARIES})
endif ()
include_directories(${SDLTTF_INCLUDE_DIR})
set(HAVE_SDL_TTF true)

# LibXML2 is required.
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

# SDL_mixer is optional, as sound is not a must-have.
find_package(SDL_mixer)

if (NOT SDLMIXER_FOUND AND SDL_mixer_FOUND)
    set(SDLMIXER_FOUND ${SDL_mixer_FOUND})
endif ()
if (NOT SDLMIXER_INCLUDE_DIR AND SDL_MIXER_INCLUDE_DIRS)
    set(SDLMIXER_INCLUDE_DIR ${SDL_MIXER_INCLUDE_DIRS})
endif ()
if (NOT SDLMIXER_LIBRARY AND SDL_MIXER_LIBRARIES)
    set(SDLMIXER_LIBRARY ${SDL_MIXER_LIBRARIES})
endif ()

if (SDLMIXER_FOUND)
    include_directories(${SDLMIXER_INCLUDE_DIR})
    set(HAVE_SDL_MIXER true)
else ()
    message(STATUS "Warning: SDL_mixer not found - sound support will be disabled")
endif ()

# Add sources.
include(src/cmake.txt)

if (MINGW)
    ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o
            COMMAND ${CMAKE_RC_COMPILER} -I${CMAKE_CURRENT_SOURCE_DIR}
            -i${CMAKE_CURRENT_SOURCE_DIR}/atrinik.rc
            -o ${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o
            VERBATIM)
    SET(SOURCES ${SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o")
    add_executable(${EXECUTABLE_UPDATER}
        src/updater/main.c "${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o")
endif ()

# Compile the sources into an executable.
add_executable(${EXECUTABLE} WIN32 ${SOURCES})
target_link_libraries(${EXECUTABLE} atrinik-toolkit)

# Link all the available libraries.
target_link_libraries(${EXECUTABLE} ${SDL_LIBRARY})
target_link_libraries(${EXECUTABLE} ${SDLIMAGE_LIBRARY})
target_link_libraries(${EXECUTABLE} ${SDLTTF_LIBRARY})
target_link_libraries(${EXECUTABLE} ${LIBXML2_LIBRARIES})

# Link SDL_mixer if available.
if (HAVE_SDL_MIXER)
    target_compile_definitions(${EXECUTABLE} PRIVATE HAVE_SDL_MIXER)
    target_link_libraries(${EXECUTABLE} ${SDLMIXER_LIBRARY})
endif ()

# Get all the include directories used to compile the client.
get_directory_property(INCLUDES INCLUDE_DIRECTORIES)

# Go through the include directories and construct a string.
foreach (var ${INCLUDES})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
    set(INCLUDES_STRING "${INCLUDES_STRING} -I${var}")
endforeach ()

# Go through the source files and construct a string.
foreach (var ${SOURCES})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" var ${var})
    set(SOURCES_STRING "${SOURCES_STRING};${var}")
endforeach ()

# Installer.
if (WIN32 AND NOT UNIX)
    set(INSTALL_SUBDIR_BIN ".")
    set(INSTALL_SUBDIR_SHARE ".")
    set(INSTALL_SUBDIR_DOC ".")

    file(GLOB DLLS "${CMAKE_CURRENT_SOURCE_DIR}/*.dll")
    set(EXES gunzip.exe tar.exe atrinik.bat ca-bundle.crt timidity.cfg)
    install(FILES ${DLLS} DESTINATION ${INSTALL_SUBDIR_BIN})
    install(TARGETS atrinik2 DESTINATION ${INSTALL_SUBDIR_BIN})
    install(FILES ${EXES} DESTINATION ${INSTALL_SUBDIR_BIN})
    install(DIRECTORY timidity DESTINATION ${INSTALL_SUBDIR_BIN})
else ()
    set(INSTALL_SUBDIR_BIN "games")
    set(INSTALL_SUBDIR_SHARE "share/games/atrinik")
    set(INSTALL_SUBDIR_DOC "share/doc/atrinik")

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/atrinik.desktop DESTINATION "share/applications")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/textures/icon.png DESTINATION "share/pixmaps" RENAME "atrinik.png")
endif ()

if (NOT WIN32 OR UNIX)
    install(TARGETS atrinik DESTINATION ${INSTALL_SUBDIR_BIN})
endif ()
install(FILES
        ${CMAKE_SOURCE_DIR}/INSTALL
        ${CMAKE_CURRENT_SOURCE_DIR}/README
        ${CMAKE_CURRENT_SOURCE_DIR}/COPYING
        ${CMAKE_CURRENT_SOURCE_DIR}/client.cfg
        DESTINATION ${INSTALL_SUBDIR_DOC})
install(DIRECTORY textures cache data fonts gfx_user schemas settings sound srv_files DESTINATION ${INSTALL_SUBDIR_SHARE} PATTERN ".git" EXCLUDE)

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fantasy multiplayer online role-playing game")
set(CPACK_PACKAGE_VENDOR "Atrinik Development Team")
set(CPACK_PACKAGE_CONTACT "Atrinik Development Team <admin@atrinik.org>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "${PACKAGE_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PACKAGE_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Atrinik")

if (MINGW)
    set(CPACK_PACKAGE_EXECUTABLES "atrinik2" "Atrinik")
    set(CPACK_CREATE_DESKTOP_LINKS "atrinik2")
endif ()

if (MINGW)
    set(CPACK_PACKAGE_FILE_NAME
        "atrinik-client-${PACKAGE_VERSION}-windows-x86_64")
endif ()

set(CPACK_DEBIAN_PACKAGE_NAME "atrinik")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libsdl-image1.2, libsdl1.2debian, libcurl4, libsdl-mixer1.2, libsdl-ttf2.0-0, timidity, zlib1g, libx11-6, libxmu6")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Fantasy multiplayer online role-playing game with 2.5D graphics\n .\n Homepage: http://www.atrinik.org/")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")

set(CPACK_RPM_PACKAGE_NAME "atrinik")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Fantasy multiplayer online role-playing game with 2.5D graphics\n .\n Homepage: http://www.atrinik.org/")
set(CPACK_RPM_PACKAGE_LICENSE "GNU General Public License (GPL)")
set(CPACK_RPM_PACKAGE_GROUP "Games")

set(CPACK_NSIS_INSTALLED_ICON_NAME "atrinik")
set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/textures/icon.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/textures/icon.ico")
set(CPACK_NSIS_DISPLAY_NAME "Atrinik")
set(CPACK_NSIS_MODIFY_PATH "ON")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

if (${PACKAGE_TYPE} STREQUAL "deb")
    set(CPACK_BINARY_DEB "ON")
elseif (${PACKAGE_TYPE} STREQUAL "nsis")
    set(CPACK_BINARY_NSIS "ON")
elseif (${PACKAGE_TYPE} STREQUAL "rpm")
    set(CPACK_BINARY_RPM "ON")
elseif (${PACKAGE_TYPE} STREQUAL "zip")
    set(CPACK_BINARY_ZIP "ON")
endif (${PACKAGE_TYPE} STREQUAL "deb")

set(CPACK_BINARY_STGZ "OFF")
set(CPACK_BINARY_TGZ "OFF")
set(CPACK_BINARY_TZ "OFF")
include(CPack)

# Configure the .h file with the configuration options (size of long,
# package version, etc).
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/include/cmake.h.def ${CMAKE_CURRENT_SOURCE_DIR}/src/include/cmake.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/include/version.h.def ${CMAKE_CURRENT_SOURCE_DIR}/src/include/version.h)
