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
    "${PROJECT_SOURCE_DIR}/src/Common"
    "${PROJECT_SOURCE_DIR}/src/Tables"
    "${PROJECT_SOURCE_DIR}/src/Storage"
)
if (DEFINED ARROW_INCLUDE_DIRS)
  target_include_directories(table_generator_tests PRIVATE ${ARROW_INCLUDE_DIRS})
endif ()
if (DEFINED YACLIB_INCLUDE_DIRS)
  target_include_directories(table_generator_tests PRIVATE ${YACLIB_INCLUDE_DIRS})
endif ()
target_link_libraries(table_generator_tests PRIVATE sch::gtest_main)
if (TARGET schengen_core)
  target_link_libraries(table_generator_tests PRIVATE schengen_core)
endif ()
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

if (TARGET schengen_storage)
  set(STORAGE_WRITER_TEST_SOURCES parquet_writer_test.cpp)
  if (TARGET schengen_scheduler)
    list(APPEND STORAGE_WRITER_TEST_SOURCES progress_test.cpp)
  endif ()
  if (TARGET sch::lance)
    list(APPEND STORAGE_WRITER_TEST_SOURCES lance_writer_test.cpp)
  endif ()
  if (TARGET sch::vortex)
    list(APPEND STORAGE_WRITER_TEST_SOURCES vortex_writer_test.cpp)
  endif ()

  add_executable(storage_writer_tests ${STORAGE_WRITER_TEST_SOURCES})
  target_compile_features(storage_writer_tests PRIVATE cxx_std_17)
  target_include_directories(
    storage_writer_tests
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src"
      "${PROJECT_SOURCE_DIR}/src/Common"
      "${PROJECT_SOURCE_DIR}/src/Tables"
      "${PROJECT_SOURCE_DIR}/src/Storage"
  )
  if (DEFINED ARROW_INCLUDE_DIRS)
    target_include_directories(storage_writer_tests PRIVATE ${ARROW_INCLUDE_DIRS})
  endif ()
  if (DEFINED YACLIB_INCLUDE_DIRS)
    target_include_directories(storage_writer_tests PRIVATE ${YACLIB_INCLUDE_DIRS})
  endif ()

  target_link_libraries(
    storage_writer_tests
    PRIVATE
      sch::gtest_main
      schengen_core
      schengen_storage
  )
  if (TARGET schengen_scheduler)
    target_link_libraries(storage_writer_tests PRIVATE schengen_scheduler)
  endif ()
  if (TARGET sch::parquet)
    target_link_libraries(storage_writer_tests PRIVATE sch::parquet)
  endif ()
  if (TARGET sch::lance)
    target_compile_definitions(storage_writer_tests PRIVATE ENABLE_LANCE=1)
    target_link_libraries(storage_writer_tests PRIVATE sch::lance)
  endif ()
  if (TARGET sch::vortex)
    target_compile_definitions(storage_writer_tests PRIVATE ENABLE_VORTEX=1)
    target_link_libraries(storage_writer_tests PRIVATE sch::vortex)
  endif ()
  if (TARGET sch::arrow)
    target_link_libraries(
      storage_writer_tests
      PRIVATE
        sch::arrow
        ${_existing_arrow_priv_deps}
    )
  endif ()

  gtest_discover_tests(
    storage_writer_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DISCOVERY_TIMEOUT 60
  )
endif ()
