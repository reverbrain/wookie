find_path(elliptics_INCLUDE_DIR elliptics/cppdef.h PATH ${ELLIPTICS_PREFIX}/include)
find_path(eblob_INCLUDE_DIR eblob/blob.h PATH ${EBLOB_PREFIX}/include)
find_path(cocaine_INCLUDE_DIR cocaine/binary.hpp PATH ${COCAINE_PREFIX}/include)
find_path(grape_INCLUDE_DIR grape/grape.hpp PATH ${GRAPE_PREFIX}/include)

find_library(elliptics_LIBRARY NAMES elliptics PATHS ${ELLIPTICS_PREFIX}/lib ${ELLIPTICS_PREFIX}/lib64)
find_library(elliptics_cpp_LIBRARY NAMES elliptics_cpp PATHS ${ELLIPTICS_PREFIX}/lib ${ELLIPTICS_PREFIX}/lib64)
find_library(eblob_LIBRARY NAMES eblob PATHS ${EBLOB_PREFIX}/lib ${EBLOB_PREFIX}/lib64)
find_library(cocaine_LIBRARY NAMES cocaine-core PATHS ${COCAINE_PREFIX}/lib ${COCAINE_PREFIX}/lib64)
find_library(grape_LIBRARY NAMES grape PATHS ${GRAPE_PREFIX}/lib ${GRAPE_PREFIX}/lib64)

set(grape_LIBRARIES ${grape_LIBRARY} ${elliptics_LIBRARY} ${elliptics_cpp_LIBRARY} ${eblob_LIBRARY} ${cocaine_LIBRARY})
set(grape_INCLUDE_DIRS ${grape_INCLUDE_DIR} ${elliptics_INCLUDE_DIR} ${eblob_INCLUDE_DIR} ${cocaine_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(grape DEFAULT_MSG grape_LIBRARIES grape_INCLUDE_DIRS)

mark_as_advanced(grape_INCLUDE_DIRS grape_LIBRARIES)
