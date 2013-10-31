cmake_minimum_required (VERSION 2.6)
project (wookie)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WOOKIE_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WOOKIE_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -std=c++11 -W -Wall -Wextra -fstack-protector-all")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Boost REQUIRED COMPONENTS system thread locale program_options)
#find_package(grape REQUIRED)
find_package(msgpack REQUIRED)
find_package(elliptics REQUIRED)
find_package(compact-language-detector-devel REQUIRED)

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(SWARM "swarm/url.hpp" "swarm")
LOCATE_LIBRARY(SWARM_XML "swarm/xml/url_finder.hpp" "swarm_xml")
LOCATE_LIBRARY(SWARM_URLFETCHER "swarm/urlfetcher/url_fetcher.hpp" "swarm_urlfetcher")
LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBXML2 "libxml/xpath.h" "xml2" "libxml2")
LOCATE_LIBRARY(LIBMAGIC "magic.h" "magic")
LOCATE_LIBRARY(THEVOID "thevoid/server.hpp" "thevoid")
LOCATE_LIBRARY(SWARM "swarm/networkmanager.h" "swarm")
LOCATE_LIBRARY(WARP "warp/lex.hpp" "")

FILE(GLOB headers
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/stem/include/stem/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/stem/include/stem/*.h"
)
install(FILES ${headers} DESTINATION include/wookie)

include_directories(${PROJECT_SOURCE_DIR}/include
	${LIBEV_INCLUDE_DIRS}	
	${SWARM_INCLUDE_DIRS}
	${grape_INCLUDE_DIRS}
	${LIBXML2_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${cld_INCLUDE_DIRS}
	${ELLIPTICS_INCLUDE_DIRS}
	${PROJECT_SOURCE_DIR}/stem/include
	${PROJECT_SOURCE_DIR}/stem/libstemmer_c/include
	${LIBMAGIC_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
)

link_directories(
	${LIBEV_LIBRARY_DIRS}	
	${SWARM_LIBRARY_DIRS}
	${grape_LIBRARY_DIRS}
	${cld_LIBRARY_DIRS}
	${LIBXML2_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
	${LIBMAGIC_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
)

add_subdirectory(lib)
add_subdirectory(stem)

# only enabled for C++
add_definitions(-std=c++0x)
add_subdirectory(grape)
add_subdirectory(test)
add_subdirectory(httpserver)
