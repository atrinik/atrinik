cmake_minimum_required (VERSION 2.6)
project (Atrinik)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake/")
#include(FindTBB.cmake)

# The version number.
set (ATRINIK_VERSION_MAJOR 0)
set (ATRINIK_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -g -O0 -DFUTURE")

find_package(Boost 1.40 COMPONENTS system thread locale filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})

if (UNIX)
    find_library(CRYPT_LIBRARY crypt REQUIRED)
endif (UNIX)

configure_file (
  "${PROJECT_SOURCE_DIR}/src/config.h.in"
  "${PROJECT_BINARY_DIR}/src/config.h"
  )

include_directories(
    "${PROJECT_BINARY_DIR}/src"
    "${PROJECT_BINARY_DIR}/src/network"
    "${PROJECT_BINARY_DIR}/src/network/messages"
    "${PROJECT_BINARY_DIR}/src/objects"
    "${PROJECT_BINARY_DIR}/src/parsers"
    "${PROJECT_BINARY_DIR}/src/types"
)

add_executable(atrinik
    src/account.cc
    src/main.cc
    src/server.cc
    src/network/commands/account.cc
    src/network/commands/setup.cc
    src/network/commands/version.cc
    src/network/game_command.cc
    src/network/game_message.cc
    src/network/game_session.cc
    src/network/game_server.cc
    src/objects/game_object.cc
    src/objects/map_object.cc
    src/objects/map_tile_object.cc
    src/objects/object.cc
    src/objects/region_object.cc
    src/parsers/account_parser.cc
    src/parsers/archetype_parser.cc
    src/parsers/map_parser.cc
    src/parsers/object_parser.cc
    src/parsers/region_parser.cc
    src/types/base_object_type.cc
    src/types/game_object_type.cc
    src/types/gfx_object_type.cc
    src/types/player_object_type.cc
    src/types/sign_object_type.cc
)

target_link_libraries(atrinik ${Boost_LIBRARIES} ${TBB_LIBRARIES}
    ${OPENSSL_LIBRARIES} ${CRYPT_LIBRARY})

install (TARGETS atrinik DESTINATION bin)
install (FILES "${PROJECT_BINARY_DIR}/config.h"
         DESTINATION include)
