cmake_minimum_required (VERSION 2.6)
project (wookie)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WOOKIE_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WOOKIE_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -std=c++0x -W -Wall -Wextra -fstack-protector-all")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Boost REQUIRED COMPONENTS system thread locale program_options)
find_package(Elliptics REQUIRED 2.25)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(SWARM "swarm/url.hpp" "swarm")
LOCATE_LIBRARY(SWARM_XML "swarm/xml/url_finder.hpp" "swarm_xml")
LOCATE_LIBRARY(SWARM_URLFETCHER "swarm/urlfetcher/url_fetcher.hpp" "swarm_urlfetcher")
LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBTIDY "tidy.h" "tidy" "tidy")
LOCATE_LIBRARY(LIBMAGIC "magic.h" "magic")
LOCATE_LIBRARY(THEVOID "thevoid/server.hpp" "thevoid")
LOCATE_LIBRARY(WARP "warp/lex.hpp" "")
LOCATE_LIBRARY(RIFT "rift/server.hpp" "rift")

FILE(GLOB headers
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.h"

)
install(FILES ${headers} DESTINATION include/wookie)

include_directories(${PROJECT_SOURCE_DIR}/include
	${LIBEV_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
	${LIBTIDY_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${ELLIPTICS_INCLUDE_DIRS}
	${LIBMAGIC_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
	${RIFT_INCLUDE_DIRS}
)

link_directories(
	${LIBEV_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
	${LIBTIDY_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
	${LIBMAGIC_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
	${RIFT_LIBRARY_DIRS}
)

add_subdirectory(lib)
add_subdirectory(processors)
add_subdirectory(test)
add_subdirectory(httpserver)
