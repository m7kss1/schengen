cmake_minimum_required(VERSION 3.25)

project(schengen LANGUAGES C CXX)

# Arrow's AWSSDK finder is invoked before it knows our in-tree AWS SDK targets
# Use custom FindAWSSDK.cmake path so Arrow can resolve the bundled SDK
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/contrib/aws-sdk-cmake/cmake")
# Arrow's Snappy finder relies on CMAKE_MODULE_PATH as well.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/contrib/snappy-cmake/cmake")
# Local cmake helpers (e.g. FindSnappy)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ENABLE_LIBRARIES "Enable building bundled third-party libraries" ON)
option(ENABLE_FORMATS "Enable format-specific targets (storage + CLI)" ON)
option(USE_SYSTEM_ARROW "Use system-installed Arrow" OFF)
option(ENABLE_PARQUET "Enable Parquet format support" ON)
option(ENABLE_ORC "Enable ORC format support (not implemented yet)" OFF)
option(ENABLE_LANCE "Enable Lance format support (not implemented yet)" OFF)
option(ENABLE_ICEBERG "Enable Iceberg format support (not implemented yet)" OFF)
option(ENABLE_VORTEX "Enable Vortex format support (not implemented yet)" OFF)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, ...)" FORCE)
endif ()

if (NOT ENABLE_FORMATS)
  message(STATUS "ENABLE_FORMATS=OFF: output formats and CLI are disabled")
  set(ENABLE_PARQUET OFF CACHE BOOL "Enable Parquet format support" FORCE)
  set(ENABLE_ORC OFF CACHE BOOL "Enable ORC format support (not implemented yet)" FORCE)
  set(ENABLE_LANCE OFF CACHE BOOL "Enable Lance format support (not implemented yet)" FORCE)
  set(ENABLE_ICEBERG OFF CACHE BOOL "Enable Iceberg format support (not implemented yet)" FORCE)
  set(ENABLE_VORTEX OFF CACHE BOOL "Enable Vortex format support (not implemented yet)" FORCE)
endif ()

set(SCHENGEN_ENABLED_FORMATS)
if (ENABLE_PARQUET)
  list(APPEND SCHENGEN_ENABLED_FORMATS parquet)
endif ()
if (ENABLE_ORC)
  message(STATUS "ENABLE_ORC=ON, but ORC support is not implemented yet; skipping")
endif ()
if (ENABLE_LANCE)
  message(STATUS "ENABLE_LANCE=ON, but Lance support is not implemented yet; skipping")
endif ()
if (ENABLE_ICEBERG)
  message(STATUS "ENABLE_ICEBERG=ON, but Iceberg support is not implemented yet; skipping")
endif ()
if (ENABLE_VORTEX)
  message(STATUS "ENABLE_VORTEX=ON, but Vortex support is not implemented yet; skipping")
endif ()

set(SCHENGEN_BUILD_FORMAT_TARGETS OFF)
if (ENABLE_FORMATS AND SCHENGEN_ENABLED_FORMATS)
  set(SCHENGEN_BUILD_FORMAT_TARGETS ON)
endif ()

if (SCHENGEN_ENABLED_FORMATS)
  list(JOIN SCHENGEN_ENABLED_FORMATS ", " _enabled_formats_message)
else ()
  set(_enabled_formats_message "none")
endif ()
message(STATUS "Enabled output formats: ${_enabled_formats_message}")
message(STATUS "Build storage/CLI targets: ${SCHENGEN_BUILD_FORMAT_TARGETS}")

set(ENABLE_ARROW ON CACHE BOOL "Enable Apache Arrow" FORCE)
set(ENABLE_THRIFT OFF CACHE BOOL "Enable Thrift" FORCE)
set(ENABLE_SNAPPY OFF CACHE BOOL "Enable Snappy" FORCE)
set(ENABLE_AWS_SDK OFF CACHE BOOL "Enable AWS SDK" FORCE)
set(ENABLE_YACLIB OFF CACHE BOOL "Enable YACLib" FORCE)
if ("parquet" IN_LIST SCHENGEN_ENABLED_FORMATS AND NOT USE_SYSTEM_ARROW)
  set(ENABLE_THRIFT ON CACHE BOOL "Enable Thrift" FORCE)
  if (EXISTS "${PROJECT_SOURCE_DIR}/contrib/snappy/CMakeLists.txt")
    set(ENABLE_SNAPPY ON CACHE BOOL "Enable Snappy" FORCE)
  else ()
    message(STATUS "Building Parquet without Snappy")
  endif ()
  if (EXISTS "${PROJECT_SOURCE_DIR}/contrib/aws-sdk-cpp/CMakeLists.txt")
    set(ENABLE_AWS_SDK ON CACHE BOOL "Enable AWS SDK" FORCE)
  else ()
    message(STATUS "Parquet enabled but aws submodule is unavailable; Disabling S3 support")
  endif ()
endif ()
if (SCHENGEN_BUILD_FORMAT_TARGETS)
  set(ENABLE_YACLIB ON CACHE BOOL "Enable YACLib" FORCE)
endif ()

if (USE_SYSTEM_ARROW)
  message(STATUS "USE_SYSTEM_ARROW=ON: using find_package(Arrow/Parquet)")
endif ()

set(ARROW_USE_EXCEPTION ON CACHE BOOL "" FORCE)
set(ARROW_PARQUET OFF CACHE BOOL "" FORCE)
if ("parquet" IN_LIST SCHENGEN_ENABLED_FORMATS)
  set(ARROW_PARQUET ON CACHE BOOL "" FORCE)
endif ()

add_subdirectory(contrib)

if (TARGET sch::gtest_main)
  enable_testing()
endif ()

if (TARGET sch::arrow)
  set(SCHENGEN_GENERATOR_SOURCES
      src/Common/dates.cpp
      src/Common/partition.cpp
      src/Common/registry.cpp
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

  add_library(schengen_core SHARED ${SCHENGEN_GENERATOR_SOURCES})
  target_compile_features(schengen_core PRIVATE cxx_std_17)
  target_include_directories(schengen_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
  if (DEFINED ARROW_INCLUDE_DIRS)
    target_include_directories(schengen_core PUBLIC ${ARROW_INCLUDE_DIRS})
  endif ()

  # Arrow static build produces multiple internal object libraries that are not
  # folded into libarrow.a. Link them explicitly to satisfy all symbols
  # TODO: FIXME
  set(_arrow_priv_deps
      arrow_array
      arrow_compute_core
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
  )

  if (SCHENGEN_BUILD_FORMAT_TARGETS)
    if ("parquet" IN_LIST SCHENGEN_ENABLED_FORMATS AND NOT TARGET sch::parquet)
      message(FATAL_ERROR "Parquet format is enabled but target is unavailable")
    endif ()

    set(SCHENGEN_STORAGE_SOURCES
        src/Storage/storage.cpp
        src/Storage/writer.cpp)

    add_library(schengen_storage STATIC ${SCHENGEN_STORAGE_SOURCES})
    target_compile_features(schengen_storage PRIVATE cxx_std_17)
    target_include_directories(schengen_storage PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if (DEFINED ARROW_INCLUDE_DIRS)
      target_include_directories(schengen_storage PRIVATE ${ARROW_INCLUDE_DIRS})
    endif ()

    set(_storage_arrow_priv_deps
        arrow_array
        arrow_compute_core
        arrow_filesystem
        arrow_io
        arrow_memory_pool
        arrow_util
        arrow_vendored)
    set(_existing_storage_arrow_priv_deps)
    foreach(dep IN LISTS _storage_arrow_priv_deps)
      if (TARGET ${dep})
        list(APPEND _existing_storage_arrow_priv_deps ${dep})
      endif ()
    endforeach()

    target_link_libraries(
      schengen_storage
      PRIVATE
        schengen_core
        sch::arrow
        sch::parquet
        ${_existing_storage_arrow_priv_deps}
    )

    if (TARGET sch::yaclib)
      find_package(Boost COMPONENTS program_options REQUIRED)

      add_executable(schengen_main src/main.cpp)
      target_compile_features(schengen_main PRIVATE cxx_std_17)
      target_include_directories(schengen_main PRIVATE ${PROJECT_SOURCE_DIR}/src)
      if (DEFINED ARROW_INCLUDE_DIRS)
        target_include_directories(schengen_main PRIVATE ${ARROW_INCLUDE_DIRS})
      endif ()
      if (DEFINED YACLIB_INCLUDE_DIRS)
        target_include_directories(schengen_main PRIVATE ${YACLIB_INCLUDE_DIRS})
      endif ()

      target_link_libraries(
        schengen_main
        PRIVATE
          schengen_core
          schengen_storage
          ${_existing_storage_arrow_priv_deps}
          Boost::program_options
          sch::arrow
          sch::parquet
          sch::yaclib
      )
    else ()
      message(STATUS "YACLib is disabled; Skipping CLI target")
    endif ()
  endif ()
else ()
  if (NOT TARGET sch::arrow)
    message(STATUS "Apache Arrow is disabled")
  endif ()
endif ()

if (TARGET sch::gtest_main AND TARGET schengen_core)
  add_subdirectory(test)
endif ()

if (TARGET schengen_core AND TARGET benchmark::benchmark_main)
  add_subdirectory(bench)
elseif (TARGET schengen_core)
  message(STATUS "Google Benchmark target is unavailable; Skipping benchmarks")
endif ()
