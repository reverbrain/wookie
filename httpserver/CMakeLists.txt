add_executable(wookie_http server.cpp)
target_link_libraries(wookie_http
	wookie
	${MSGPACK_LIBRARIES}
	${THEVOID_LIBRARIES}
	${RIFT_LIBRARIES}
	${SWARM_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${Boost_LIBRARIES}
)
