find_path(msgpack_INCLUDE_DIR msgpack.hpp PATH ${GRAPE_PREFIX}/include)

find_library(msgpack_LIBRARY NAMES msgpack PATHS ${GRAPE_PREFIX}/lib ${GRAPE_PREFIX}/lib64)

set(msgpack_LIBRARIES ${msgpack_LIBRARY})
set(msgpack_INCLUDE_DIRS ${msgpack_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(msgpack DEFAULT_MSG msgpack_LIBRARIES msgpack_INCLUDE_DIRS)

mark_as_advanced(msgpack_INCLUDE_DIRS msgpack_LIBRARIES)
