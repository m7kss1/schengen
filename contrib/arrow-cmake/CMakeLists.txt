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
set(ARROW_INSTALL OFF CACHE BOOL "Disable Arrow install/export targets" FORCE)
set(ARROW_FILESYSTEM ON CACHE BOOL "" FORCE)
set(ARROW_IPC ON CACHE BOOL "" FORCE)
set(ARROW_COMPUTE ON CACHE BOOL "" FORCE)
set(ARROW_SIMD_LEVEL "NONE" CACHE STRING "Disable Arrow SIMD to ease portability" FORCE)
set(ARROW_RUNTIME_SIMD_LEVEL "NONE" CACHE STRING "Disable runtime SIMD dispatch" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip install rules for embedded Arrow" FORCE)

find_package(ZLIB REQUIRED)

# Arrow's ThirdpartyToolchain expects Thrift_VERSION to be non-empty when
# ARROW_WITH_THRIFT is enabled. Provide a default to avoid CMake errors when
# using our vendored Thrift target.
if (ARROW_PARQUET AND (NOT DEFINED Thrift_VERSION OR Thrift_VERSION STREQUAL ""))
  set(Thrift_VERSION "0.22.0")
endif ()

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

if (TARGET parquet_static)
  add_library(sch::parquet ALIAS parquet_static)
endif ()
