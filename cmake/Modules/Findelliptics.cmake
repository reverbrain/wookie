find_path(ELLIPTICS_INCLUDE_DIR elliptics/cppdef.h PATHS ${ELLIPTICS_PREFIX}/include /usr/include)

find_library(ELLIPTICS_cpp_LIBRARY NAMES elliptics_cpp PATHS ${ELLIPTICS_PREFIX}/lib ${ELLIPTICS_PREFIX}/lib64 /usr/lib /usr/lib64)

set(ELLIPTICS_LIBRARIES ${ELLIPTICS_cpp_LIBRARY})
set(ELLIPTICS_INCLUDE_DIRS ${ELLIPTICS_INCLUDE_DIR})

message("elliptics: libs: ${ELLIPTICS_LIBRARIES}, includes: ${ELLIPTICS_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(elliptics DEFAULT_MSG	ELLIPTICS_LIBRARIES ELLIPTICS_INCLUDE_DIRS)

mark_as_advanced(ELLIPTICS_INCLUDE_DIRS ELLIPTICS_LIBRARIES)
