cmake_minimum_required(VERSION 3.25)

project(schengen LANGUAGES C CXX)

set(ARROW_USE_EXCEPTION ON CACHE BOOL "" FORCE)
set(ARROW_S3 OFF CACHE BOOL "" FORCE)
set(ARROW_PARQUET ON CACHE BOOL "" FORCE)

option(ENABLE_LIBRARIES "Enable building bundled third-party libraries" ON)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, ...)" FORCE)
endif ()

add_subdirectory(contrib)

if (TARGET sch::gtest_main)
  enable_testing()
endif ()

add_subdirectory(test)

if (TARGET sch::arrow AND TARGET sch::yaclib)
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
    schengen_main
    PRIVATE
      ${_existing_arrow_priv_deps}
      sch::arrow
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
