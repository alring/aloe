find_library(PLPLOT_LIBRARY NAMES plplotd)
set(PLPLOT_LIBRARIES ${PLPLOT_LIBRARY})
find_path(PLPLOT_INCLUDE_DIR NAMES plplot/plplot.h)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PLPLOT DEFAULT_MSG PLPLOT_LIBRARIES PLPLOT_INCLUDE_DIR)

mark_as_advanced(
PLPLOT_LIBRARY
PLPLOTCXX_LIBRARY
PLPLOT_LIBRARIES
PLPLOT_INCLUDE_DIR 
)
