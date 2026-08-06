# The Atrinik CMakeLists file.

set(EXECUTABLE atrinik)
set(PACKAGE_NAME "Atrinik Client")

# SDL3 is required to run the client. Use its exported CMake targets so Linux
# and MinGW builds share the same dependency contract.
find_package(SDL3 3.4 CONFIG REQUIRED)
set(HAVE_SDL true)

# SDL_image is also required, otherwise no bitmaps or map images could be
# displayed.
find_package(SDL3_image 3.2 CONFIG REQUIRED)
set(HAVE_SDL_IMAGE true)

# SDL_ttf is used for all text drawing, so it's required.
find_package(SDL3_ttf 3.2 CONFIG REQUIRED)
set(HAVE_SDL_TTF true)

# LibXML2 is required.
find_package(LibXml2 REQUIRED)

# Sound effects and music are core client functionality on every platform.
find_package(SDL3_mixer 3.2.4 CONFIG REQUIRED)
set(HAVE_SDL_MIXER true)

# Add sources.
include(src/cmake.txt)

if (MINGW)
    ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o
            COMMAND ${CMAKE_RC_COMPILER} -I${CMAKE_CURRENT_SOURCE_DIR}
            -i${CMAKE_CURRENT_SOURCE_DIR}/atrinik.rc
            -o ${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o
            VERBATIM)
    SET(SOURCES ${SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/atrinikrc.o")
endif ()

# Compile the sources into an executable.
add_executable(${EXECUTABLE} WIN32 ${SOURCES})
atrinik_configure_target(${EXECUTABLE})
target_compile_definitions(${EXECUTABLE} PRIVATE SDL_DISABLE_OLD_NAMES)
target_include_directories(${EXECUTABLE} BEFORE PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/toolkit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBXML2_INCLUDE_DIR})

target_link_libraries(${EXECUTABLE} PRIVATE atrinik-toolkit)

# Link all the available libraries.
target_link_libraries(${EXECUTABLE} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf)
if (TARGET LibXml2::LibXml2)
    target_link_libraries(${EXECUTABLE} PRIVATE LibXml2::LibXml2)
else ()
    target_link_libraries(${EXECUTABLE} PRIVATE ${LIBXML2_LIBRARIES})
endif ()

# Sound effects and music are required on every supported platform.
target_compile_definitions(${EXECUTABLE} PRIVATE HAVE_SDL_MIXER)
target_link_libraries(${EXECUTABLE} PRIVATE SDL3_mixer::SDL3_mixer)

if (MINGW)
    if (NOT CMAKE_OBJDUMP)
        message(FATAL_ERROR
            "CMAKE_OBJDUMP is required to validate Windows client sound support")
    endif ()

    add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DATRINIK_OBJDUMP=${CMAKE_OBJDUMP}"
            "-DATRINIK_CLIENT_EXECUTABLE=$<TARGET_FILE:${EXECUTABLE}>"
            -P "${CMAKE_SOURCE_DIR}/cmake/verify_windows_client_sound.cmake"
        COMMENT "Verifying that the Windows client imports SDL3_mixer"
        VERBATIM)
endif ()

if (BUILD_TESTING AND NOT MINGW)
    add_executable(client-packet-payload-tests
        src/tests/packet_payload.c
        src/client/packet_payload.c)
    atrinik_configure_target(client-packet-payload-tests)
    target_include_directories(client-packet-payload-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
    target_link_libraries(client-packet-payload-tests PRIVATE atrinik-toolkit)
    add_test(NAME client-packet-payload COMMAND client-packet-payload-tests)

    add_executable(client-surface-primitives-tests
        src/tests/surface_primitives.c
        src/gui/toolkit/surface_primitives.c)
    atrinik_configure_target(client-surface-primitives-tests)
    target_compile_definitions(client-surface-primitives-tests PRIVATE SDL_DISABLE_OLD_NAMES)
    target_include_directories(client-surface-primitives-tests BEFORE PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/toolkit/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(client-surface-primitives-tests PRIVATE atrinik-toolkit SDL3::SDL3)
    add_test(NAME client-surface-primitives COMMAND client-surface-primitives-tests)
endif ()

# Installer.
if (WIN32 AND NOT UNIX)
    set(INSTALL_SUBDIR_BIN ".")
    set(INSTALL_SUBDIR_SHARE ".")
    set(INSTALL_SUBDIR_DOC ".")

    set(ATRINIK_WINDOWS_RUNTIME_DIR "" CACHE PATH
        "Directory containing MinGW runtime DLLs for portable packages")
    set(ATRINIK_CA_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/ca-bundle.crt" CACHE FILEPATH
        "CA certificate bundle to install with the client")

    install(TARGETS atrinik
        RUNTIME DESTINATION ${INSTALL_SUBDIR_BIN})

    if (ATRINIK_WINDOWS_RUNTIME_DIR)
        file(GLOB ATRINIK_WINDOWS_RUNTIME_DLLS
            "${ATRINIK_WINDOWS_RUNTIME_DIR}/*.dll")

        if (ATRINIK_WINDOWS_RUNTIME_DLLS)
            install(FILES ${ATRINIK_WINDOWS_RUNTIME_DLLS}
                DESTINATION ${INSTALL_SUBDIR_BIN})
        else ()
            message(FATAL_ERROR
                "No DLLs found in ATRINIK_WINDOWS_RUNTIME_DIR: ${ATRINIK_WINDOWS_RUNTIME_DIR}")
        endif ()
    elseif (PACKAGE_TYPE STREQUAL "zip")
        message(WARNING "ATRINIK_WINDOWS_RUNTIME_DIR is empty; portable packages will not include dependency DLLs")
    endif ()

    install(FILES "${ATRINIK_CA_BUNDLE}" DESTINATION ${INSTALL_SUBDIR_BIN}
        RENAME ca-bundle.crt)

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/timidity.cfg")
        install(FILES timidity.cfg DESTINATION ${INSTALL_SUBDIR_BIN})
    endif ()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/timidity")
        install(DIRECTORY timidity DESTINATION ${INSTALL_SUBDIR_BIN})
    endif ()
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
install(DIRECTORY
        textures cache data fonts gfx_user settings sound srv_files
        DESTINATION ${INSTALL_SUBDIR_SHARE}
        PATTERN ".git" EXCLUDE)

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
    set(CPACK_PACKAGE_EXECUTABLES "atrinik" "Atrinik")
    set(CPACK_CREATE_DESKTOP_LINKS "atrinik")
endif ()

if (MINGW)
    set(CPACK_PACKAGE_FILE_NAME
        "atrinik-client-${PACKAGE_VERSION}-windows-x86_64")
endif ()

set(CPACK_DEBIAN_PACKAGE_NAME "atrinik")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libsdl3-0, libsdl3-image0, libsdl3-ttf0, libcurl4, timidity, zlib1g")
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
configure_file(src/include/cmake.h.def src/include/cmake.h)
configure_file(src/include/version.h.def src/include/version.h)
