cmake_minimum_required (VERSION 2.6)
project (wookie)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WOOKIE_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WOOKIE_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -W -Wall -Wextra -fstack-protector-all")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Boost REQUIRED COMPONENTS system thread locale)
find_package(grape REQUIRED)
find_package(msgpack REQUIRED)
find_package(compact-language-detector-devel REQUIRED)

FILE(GLOB headers
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/wookie/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/stem/include/stem/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/stem/include/stem/*.h"
)
install(FILES ${headers} DESTINATION include/wookie)

include_directories(${PROJECT_SOURCE_DIR}
	${grape_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}
	"${PROJECT_SOURCE_DIR}/include"
	${cld_INCLUDE_DIRS}
	"${PROJECT_SOURCE_DIR}/src/stem/include"
	"${PROJECT_SOURCE_DIR}/src/stem/libstemmer_c/include"
)

add_subdirectory(src)
