FILE(GLOB stemmers "libstemmer_c/src_c/*.c" libstemmer_c/runtime/api.c libstemmer_c/runtime/utilities.c libstemmer_c/libstemmer/libstemmer.c)

add_library(wookie_stem SHARED src/stem.cpp ${stemmers})

target_link_libraries(wookie_stem ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
set_target_properties(wookie_stem PROPERTIES VERSION ${WOOKIE_VERSION_MAJOR} SOVERSION ${WOOKIE_VERSION_MAJOR})
