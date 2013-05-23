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
	${ELLIPTICS_LIBRARIES}
)

add_executable(wookie_document_reader document_reader.cpp)
target_link_libraries(wookie_document_reader
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${elliptics_cpp_LIBRARY}
	${elliptics_client_LIBRARY}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
)
