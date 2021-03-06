if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree build. This is bad practice.")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

cmake_minimum_required (VERSION 2.6)
project (ALOE++)

# The version number.
set (ALOE_VERSION_MAJOR 0)
set (ALOE_VERSION_MINOR 2)

set(MODULE_REPOS_NAME "default")

if(DEBUG)
	message("-- Configuring debugging CFLAGS")
	set(CFDEB "-O0 -g -rdynamic")
else()
	set(CFDEB "-O2")
endif()

set(CMAKE_C_FLAGS "${CFDEB} -Wall -Winline -Wno-unused-result -Wno-format -std=c99 -D_GNU_SOURCE -fPIC")
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(RTDAL_INCLUDE "")
list(APPEND RTDAL_INCLUDE ${CMAKE_SOURCE_DIR}/rtdal_lnx/include)
list(APPEND RTDAL_INCLUDE ${CMAKE_SOURCE_DIR}/rtdal_lnx/extern)
set(OESR_INCLUDE ${CMAKE_SOURCE_DIR}/oesr/include)

add_subdirectory(oesr)
add_subdirectory(rtdal_lnx)
add_subdirectory(modrep_default)
add_subdirectory(modrep_osld)
