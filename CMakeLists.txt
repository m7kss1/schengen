cmake_minimum_required(VERSION 3.25)

project(yatpchgen LANGUAGES CXX)

option(ENABLE_LIBRARIES "Enable building bundled third-party libraries" ON)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, ...)" FORCE)
endif ()

add_subdirectory(contrib)

if (TARGET yatpchgen::arrow AND TARGET yatpchgen::yaclib)
  add_executable(yatpchgen_main src/main.cpp)
  target_compile_features(yatpchgen_main PRIVATE cxx_std_17)

  if (DEFINED YATPCHGEN_ARROW_INCLUDE_DIRS)
    target_include_directories(yatpchgen_main PRIVATE ${YATPCHGEN_ARROW_INCLUDE_DIRS})
  endif ()
  if (DEFINED YATPCHGEN_YACLIB_INCLUDE_DIRS)
    target_include_directories(yatpchgen_main PRIVATE ${YATPCHGEN_YACLIB_INCLUDE_DIRS})
  endif ()

  # Arrow static build produces multiple internal object libraries that are not
  # folded into libarrow.a. Link them explicitly to satisfy all symbols.
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
    yatpchgen_main
    PRIVATE
      ${_existing_arrow_priv_deps}
      yatpchgen::arrow
      yatpchgen::yaclib
  )
else ()
  if (NOT TARGET yatpchgen::arrow)
    message(STATUS "Apache Arrow is disabled")
  endif ()
  if (NOT TARGET yatpchgen::yaclib)
    message(STATUS "YACLib is disabled")
  endif ()
endif ()
