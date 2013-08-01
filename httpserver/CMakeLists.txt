add_executable(wookie_http server.cpp)
target_link_libraries(wookie_http
	elliptics-thevoid
	wookie
	wookie_stem
	${THEVOID_LIBRARIES}
	${SWARM_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${Boost_LIBRARIES}
)
