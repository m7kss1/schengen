option(ENABLE_ARROW "Enable building Apache Arrow" ${ENABLE_LIBRARIES})

if (NOT ENABLE_ARROW)
  message(STATUS "Not using Apache Arrow (set ENABLE_ARROW=ON to enable)")
  return()
endif ()

# Ensure imported targets from system packages are visible outside this subdirectory
if (USE_SYSTEM_ARROW)
  set(_sch_prev_find_package_targets_global "${CMAKE_FIND_PACKAGE_TARGETS_GLOBAL}")
  set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
  find_package(Arrow REQUIRED CONFIG)
  set(_sch_arrow_candidates
      Arrow::arrow
      Arrow::arrow_shared
      Arrow::arrow_static
      arrow_shared
      arrow_static)
  set(_sch_arrow_target "")
  foreach(candidate IN LISTS _sch_arrow_candidates)
    if (TARGET ${candidate})
      set(_sch_arrow_target ${candidate})
      break()
    endif ()
  endforeach()
  if (_sch_arrow_target STREQUAL "")
    message(FATAL_ERROR "USE_SYSTEM_ARROW=ON but no Arrow target was found")
  endif ()

  get_target_property(_sch_arrow_includes ${_sch_arrow_target} INTERFACE_INCLUDE_DIRECTORIES)
  if (_sch_arrow_includes)
    set(ARROW_INCLUDE_DIRS "${_sch_arrow_includes}" CACHE INTERNAL "Arrow include directories" FORCE)
  endif ()
  add_library(sch::arrow ALIAS ${_sch_arrow_target})

  if (ENABLE_PARQUET)
    find_package(Parquet REQUIRED CONFIG)
    set(_sch_parquet_candidates
        Parquet::parquet
        Parquet::parquet_shared
        Parquet::parquet_static
        parquet_shared
        parquet_static)
    set(_sch_parquet_target "")
    foreach(candidate IN LISTS _sch_parquet_candidates)
      if (TARGET ${candidate})
        set(_sch_parquet_target ${candidate})
        break()
      endif ()
    endforeach()
    if (_sch_parquet_target STREQUAL "")
      message(FATAL_ERROR "ENABLE_PARQUET=ON but no Parquet target was found in system packages")
    endif ()
    add_library(sch::parquet ALIAS ${_sch_parquet_target})
  endif ()

  set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL "${_sch_prev_find_package_targets_global}")

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
set(ARROW_COMPUTE ON CACHE BOOL "" FORCE)
set(ARROW_SIMD_LEVEL "NONE" CACHE STRING "Disable Arrow SIMD to ease portability" FORCE)
set(ARROW_RUNTIME_SIMD_LEVEL "NONE" CACHE STRING "Disable runtime SIMD dispatch" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip install rules for embedded Arrow" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build bundled dependencies with -fPIC" FORCE)

set(_schengen_enable_parquet OFF)
if (ENABLE_PARQUET)
  set(_schengen_enable_parquet ON)
endif ()

set(ARROW_PARQUET ${_schengen_enable_parquet} CACHE BOOL "Enable Parquet support in Arrow" FORCE)
set(ARROW_FILESYSTEM ${_schengen_enable_parquet} CACHE BOOL "Enable Arrow filesystem layer" FORCE)
set(ARROW_IPC ON CACHE BOOL "Enable Arrow IPC support" FORCE)
set(_schengen_enable_s3 OFF)
if (_schengen_enable_parquet AND ENABLE_AWS_SDK)
  set(_schengen_enable_s3 ON)
endif ()
set(_schengen_enable_snappy OFF)
if (_schengen_enable_parquet AND ENABLE_SNAPPY)
  set(_schengen_enable_snappy ON)
endif ()

set(ARROW_S3 ${_schengen_enable_s3} CACHE BOOL "Enable Arrow S3 support" FORCE)
set(ARROW_WITH_SNAPPY ${_schengen_enable_snappy} CACHE BOOL "Enable Snappy codec in Arrow" FORCE)
if (_schengen_enable_snappy)
  set(Snappy_SOURCE "SYSTEM" CACHE STRING "Use in-tree snappy" FORCE)
else ()
  set(Snappy_SOURCE "AUTO" CACHE STRING "Autodetect Snappy source" FORCE)
endif ()
if (_schengen_enable_s3)
  set(AWSSDK_SOURCE "SYSTEM" CACHE STRING "Use bundled AWS SDK" FORCE)
else ()
  set(AWSSDK_SOURCE "AUTO" CACHE STRING "Autodetect AWS SDK source" FORCE)
endif ()

find_package(ZLIB REQUIRED)

# Arrow's ThirdpartyToolchain expects Thrift_VERSION to be non-empty when
# ARROW_WITH_THRIFT is enabled. Provide a default to avoid CMake errors
if (_schengen_enable_parquet AND (NOT DEFINED Thrift_VERSION OR Thrift_VERSION STREQUAL ""))
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
