add_executable(wookie_stem_test stem_test.cpp)
target_link_libraries(wookie_stem_test wookie_stem)

add_executable(wookie_rindex_test rindex.cpp)
target_link_libraries(wookie_rindex_test
	wookie
	wookie_stem
	${SWARM_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${LIBMAGIC_LIBRARIES}
	${Boost_LIBRARIES}
)

add_executable(wookie_document_reader document_reader.cpp)
target_link_libraries(wookie_document_reader
	wookie
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${elliptics_cpp_LIBRARY}
	${elliptics_client_LIBRARY}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	-pthread
)

add_executable(wookie_data_iterator iterator.cpp)
target_link_libraries(wookie_data_iterator
	wookie
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${elliptics_cpp_LIBRARY}
	${elliptics_client_LIBRARY}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	-pthread
)

add_executable(wookie_swarm_download swarm.cpp)
target_link_libraries(wookie_swarm_download
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${SWARM_LIBRARIES}
	${SWARM_URLFETCHER_LIBRARIES}
)

add_executable(wookie_encoding encoding.cpp)
target_link_libraries(wookie_encoding
	${Boost_LIBRARIES}
)

add_executable(wookie_fuzzy fuzzy.cpp)
target_link_libraries(wookie_fuzzy
	${Boost_LIBRARIES}
)

add_subdirectory(similarity)
