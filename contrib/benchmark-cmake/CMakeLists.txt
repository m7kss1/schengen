option(ENABLE_BENCHMARK "Enable Google Benchmark" ${ENABLE_LIBRARIES})

if (NOT ENABLE_BENCHMARK)
  message(STATUS "Not using Google Benchmark")
  return()
endif ()

set(BENCHMARK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../benchmark")

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable Google Benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable Google Benchmark gtest tests" FORCE)
set(BENCHMARK_ENABLE_ASSEMBLY_TESTS OFF CACHE BOOL "Disable Google Benchmark assembly tests" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable Google Benchmark install targets" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "Disable Google Benchmark docs install" FORCE)
set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "Disable Google Benchmark tools install" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable Google Benchmark -Werror" FORCE)
set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "Disable exceptions in Google Benchmark" FORCE)

add_subdirectory(
  "${BENCHMARK_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/benchmark-build"
)

if (TARGET benchmark)
  add_library(sch::benchmark ALIAS benchmark)
endif ()

if (TARGET benchmark_main)
  add_library(sch::benchmark_main ALIAS benchmark_main)
endif ()
