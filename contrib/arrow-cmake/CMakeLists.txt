option(ENABLE_ARROW "Enable building Apache Arrow" ${ENABLE_LIBRARIES})

if (NOT ENABLE_ARROW)
  message(STATUS "Not using Apache Arrow (set ENABLE_ARROW=ON to enable)")
  return()
endif ()

set(ARROW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../arrow/cpp")

set(ARROW_BUILD_SHARED OFF CACHE BOOL "Disable Arrow shared libraries" FORCE)
set(ARROW_BUILD_STATIC ON CACHE BOOL "Enable Arrow static libraries" FORCE)
set(ARROW_BUILD_TESTS OFF CACHE BOOL "Disable Arrow tests" FORCE)
set(ARROW_BUILD_BENCHMARKS OFF CACHE BOOL "Disable Arrow benchmarks" FORCE)
set(ARROW_BUILD_EXAMPLES OFF CACHE BOOL "Disable Arrow examples" FORCE)
set(ARROW_BUILD_INTEGRATION OFF CACHE BOOL "Disable Arrow integration builds" FORCE)
set(ARROW_BUILD_UTILITIES OFF CACHE BOOL "Disable Arrow CLI utilities" FORCE)
set(ARROW_SIMD_LEVEL "NONE" CACHE STRING "Disable Arrow SIMD to ease portability" FORCE)
set(ARROW_RUNTIME_SIMD_LEVEL "NONE" CACHE STRING "Disable runtime SIMD dispatch" FORCE)

set(ARROW_INCLUDE_DIRS
    "${ARROW_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/arrow-build/src"
    CACHE INTERNAL "Arrow include directories" FORCE)

add_subdirectory(
  "${ARROW_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/arrow-build"
)

if (TARGET arrow_static)
  add_library(sch::arrow ALIAS arrow_static)
endif ()
