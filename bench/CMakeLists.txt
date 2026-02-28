if (NOT TARGET benchmark::benchmark_main)
  message(STATUS "Google Benchmark target is unavailable; Skipping")
  return()
endif ()

add_executable(table_iter_bench EXCLUDE_FROM_ALL tables.cpp)
target_compile_features(table_iter_bench PRIVATE cxx_std_17)

target_link_libraries(
  table_iter_bench
  PRIVATE
    benchmark::benchmark_main
    schengen_core
)
