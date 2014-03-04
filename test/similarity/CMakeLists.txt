INCLUDE(${CMAKE_SOURCE_DIR}/cmake/locate_library.cmake)
LOCATE_LIBRARY(DLIB "dlib/svm.h" "")
include_directories(${DLIB_INCLUDE_DIRS})

add_executable(wookie_html_parser html_parser.cpp)
target_link_libraries(wookie_html_parser
	${LIBTIDY_LIBRARIES}
	${Boost_LIBRARIES}
	${LIBMAGIC_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
)

add_executable(wookie_elliptics_parse_docs elliptics_parse_docs.cpp)
target_link_libraries(wookie_elliptics_parse_docs
	${LIBTIDY_LIBRARIES}
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${LIBMAGIC_LIBRARIES}
	${SWARM_LIBRARIES}
	${SWARM_URLFETCHER_LIBRARIES}
)

add_executable(wookie_elliptics_load_docs elliptics_load_docs.cpp)
target_link_libraries(wookie_elliptics_load_docs
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
)


