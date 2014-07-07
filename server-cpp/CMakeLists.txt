cmake_minimum_required (VERSION 2.6)
project (Atrinik)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake/")
#include(FindTBB.cmake)

# The version number.
set (ATRINIK_VERSION_MAJOR 0)
set (ATRINIK_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -g -O0 -DFUTURE")

find_package(Boost 1.40 COMPONENTS system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

include_directories ("${PROJECT_BINARY_DIR}")

add_executable(atrinik game_object.cc object.cc game_object_type.cc
    object_parser.cc main.cc archetype_parser.cc map_object.cc map_parser.cc
    sign_object_type.cc map_tile_object.cc gfx_object_type.cc
    base_object_type.cc region_object.cc region_parser.cc)

target_link_libraries(atrinik ${Boost_LIBRARIES} ${TBB_LIBRARIES})

install (TARGETS atrinik DESTINATION bin)
install (FILES "${PROJECT_BINARY_DIR}/config.h"
         DESTINATION include)
