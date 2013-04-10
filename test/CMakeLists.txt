add_executable(wookie_stem_test stem_test.cpp)
target_link_libraries(wookie_stem_test wookie_stem)

add_executable(wookie_rindex_test rindex.cpp)
target_link_libraries(wookie_rindex_test
	wookie_stem
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_LOCALE_LIBRARY}
	${elliptics_cpp_LIBRARY}
	${cld_LIBRARIES}
	${LIBEV_LIBRARIES}
	${SWARM_LIBRARIES}
	${LIBXML2_LIBRARIES}
	${MSGPACK_LIBRARIES}
)
