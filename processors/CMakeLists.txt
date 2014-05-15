LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")

find_package(CocaineNative REQUIRED)
include_directories(${CocaineNative_INCLUDE_DIRS} ${LIBEV_INCLUDE_DIRS})

set(PROCESSOR_GENERIC_LIBRARIES
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${CocaineNative_LIBRARIES}
	${LIBEV_LIBRARIES}
)

add_executable(first_processor first_processor.cpp)
target_link_libraries(first_processor
	${PROCESSOR_GENERIC_LIBRARIES}
	${SWARM_LIBRARIES}
)

add_executable(stub_processor stub_processor.cpp)
target_link_libraries(stub_processor
	${PROCESSOR_GENERIC_LIBRARIES}
)

add_executable(html_processor html_processor.cpp)
target_link_libraries(html_processor
	${PROCESSOR_GENERIC_LIBRARIES}
)

add_executable(feature_extractor feature_extractor.cpp)
target_link_libraries(feature_extractor
	${PROCESSOR_GENERIC_LIBRARIES}
)

add_executable(last_processor last_processor.cpp)
target_link_libraries(last_processor
	${PROCESSOR_GENERIC_LIBRARIES}
)
