if (NOT TARGET sch::gtest_main)
  message(STATUS "GoogleTest is disabled; skipping unit tests")
  return()
endif ()

add_executable(table_generator_tests table_generator.cpp)
target_compile_features(table_generator_tests PRIVATE cxx_std_17)
target_include_directories(
  table_generator_tests
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)
if (DEFINED ARROW_INCLUDE_DIRS)
  target_include_directories(table_generator_tests PRIVATE ${ARROW_INCLUDE_DIRS})
endif ()
if (DEFINED YACLIB_INCLUDE_DIRS)
  target_include_directories(table_generator_tests PRIVATE ${YACLIB_INCLUDE_DIRS})
endif ()
target_link_libraries(table_generator_tests PRIVATE sch::gtest_main)
if (TARGET sch::arrow)
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
    table_generator_tests
    PRIVATE
      sch::arrow
      ${_existing_arrow_priv_deps}
  )
endif ()

include(GoogleTest)
gtest_discover_tests(
  table_generator_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DISCOVERY_TIMEOUT 60
)
