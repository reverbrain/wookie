find_path(cld_INCLUDE_DIR cld/compact_lang_det.h PATH ${CLD_PREFIX}/include)
find_library(cld_LIBRARY NAMES cld PATHS ${CLD_PREFIX}/lib ${CLD_PREFIX}/lib64)

set(cld_LIBRARIES ${cld_LIBRARY})
set(cld_INCLUDE_DIRS ${cld_INCLUDE_DIR} "${cld_INCLUDE_DIR}/cld")

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(cld DEFAULT_MSG cld_LIBRARIES cld_INCLUDE_DIRS)

mark_as_advanced(cld_INCLUDE_DIRS cld_LIBRARIES)
