aux_source_directory(. WOOKIE_SRC_LIST)
add_library(wookie SHARED ${WOOKIE_SRC_LIST})

target_link_libraries(wookie
	${Boost_LIBRARIES}
	${elliptics_cpp_LIBRARY}
	${SWARM_LIBRARIES}
	${SWARM_XML_LIBRARIES}
	${SWARM_URLFETCHER_LIBRARIES}
	${LIBTIDY_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${LIBMAGIC_LIBRARIES}
)
