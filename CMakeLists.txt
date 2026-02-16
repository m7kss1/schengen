cmake_minimum_required(VERSION 3.25)

project(schengen LANGUAGES C CXX)

# Arrow's AWSSDK finder is invoked before it knows our in-tree AWS SDK targets
# Use custom FindAWSSDK.cmake path so Arrow can resolve the bundled SDK
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/contrib/aws-sdk-cmake/cmake")
# Arrow's Snappy finder relies on CMAKE_MODULE_PATH as well.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/contrib/snappy-cmake/cmake")
# Local cmake helpers (e.g. FindSnappy)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(ARROW_USE_EXCEPTION ON CACHE BOOL "" FORCE)
set(ARROW_S3 ON CACHE BOOL "" FORCE)
set(ARROW_PARQUET ON CACHE BOOL "" FORCE)

option(ENABLE_LIBRARIES "Enable building bundled third-party libraries" ON)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, ...)" FORCE)
endif ()

find_package(Boost COMPONENTS program_options REQUIRED)

add_subdirectory(contrib)

if (TARGET sch::gtest_main)
  enable_testing()
endif ()

if (TARGET sch::arrow AND TARGET sch::yaclib)
  set(SCHENGEN_CORE_SOURCES
      src/Common/dates.cpp
      src/Common/partition.cpp
      src/Common/registry.cpp
      src/Storage/storage.cpp
      src/Storage/writer.cpp
      src/Tables/customer.cpp
      src/Tables/lineitem.cpp
      src/Tables/nation.cpp
      src/Tables/orders.cpp
      src/Tables/part.cpp
      src/Tables/partsupp.cpp
      src/Tables/region.cpp
      src/Tables/register_tables.cpp
      src/Tables/supplier.cpp
      src/Tables/table.cpp)

  add_library(schengen_core STATIC ${SCHENGEN_CORE_SOURCES})
  target_compile_features(schengen_core PRIVATE cxx_std_17)
  target_include_directories(schengen_core PRIVATE ${PROJECT_SOURCE_DIR}/src)
  if (DEFINED ARROW_INCLUDE_DIRS)
    target_include_directories(schengen_core PRIVATE ${ARROW_INCLUDE_DIRS})
  endif ()
  if (DEFINED YACLIB_INCLUDE_DIRS)
    target_include_directories(schengen_core PRIVATE ${YACLIB_INCLUDE_DIRS})
  endif ()

  add_executable(schengen_main src/main.cpp)
  target_compile_features(schengen_main PRIVATE cxx_std_17)

  target_include_directories(schengen_main PRIVATE ${PROJECT_SOURCE_DIR}/src)
  if (DEFINED ARROW_INCLUDE_DIRS)
    target_include_directories(schengen_main PRIVATE ${ARROW_INCLUDE_DIRS})
  endif ()
  if (DEFINED YACLIB_INCLUDE_DIRS)
    target_include_directories(schengen_main PRIVATE ${YACLIB_INCLUDE_DIRS})
  endif ()

  # Arrow static build produces multiple internal object libraries that are not
  # folded into libarrow.a. Link them explicitly to satisfy all symbols
  # TODO: FIXME
  set(_arrow_priv_deps
      arrow_array
      arrow_compute_core
      arrow_filesystem
      arrow_io
      arrow_memory_pool
      arrow_util
      arrow_vendored)
  set(_existing_arrow_priv_deps)
  foreach(dep IN LISTS _arrow_priv_deps)
    if (TARGET ${dep})
      list(APPEND _existing_arrow_priv_deps ${dep})
    endif ()
  endforeach()

  target_link_libraries(
    schengen_core
    PRIVATE
      ${_existing_arrow_priv_deps}
      sch::arrow
      sch::parquet
  )

  target_link_libraries(
    schengen_main
    PRIVATE
      schengen_core
      ${_existing_arrow_priv_deps}
      Boost::program_options
      sch::arrow
      sch::parquet
      sch::yaclib
  )
else ()
  if (NOT TARGET sch::arrow)
    message(STATUS "Apache Arrow is disabled")
  endif ()
  if (NOT TARGET sch::yaclib)
    message(STATUS "YACLib is disabled")
  endif ()
endif ()

if (TARGET sch::gtest_main)
  add_subdirectory(test)
endif ()

if (TARGET schengen_core)
  add_subdirectory(bench)
endif ()
