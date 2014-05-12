LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")

find_package(CocaineNative REQUIRED)
include_directories(${CocaineNative_INCLUDE_DIRS} ${LIBEV_INCLUDE_DIRS})

add_executable(first_processor first_processor.cpp)

target_link_libraries(first_processor
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${CocaineNative_LIBRARIES}
	${LIBEV_LIBRARIES}
	${SWARM_LIBRARIES}
)
