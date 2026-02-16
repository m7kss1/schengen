if (NOT TARGET benchmark::benchmark_main)
  message(FATAL_ERROR "Google Benchmark target is unavailable")
endif ()

add_executable(table_iter_bench EXCLUDE_FROM_ALL tables.cpp)
target_compile_features(table_iter_bench PRIVATE cxx_std_17)
target_include_directories(table_iter_bench PRIVATE "${PROJECT_SOURCE_DIR}/src")
if (DEFINED ARROW_INCLUDE_DIRS)
  target_include_directories(table_iter_bench PRIVATE ${ARROW_INCLUDE_DIRS})
endif ()

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
  table_iter_bench
  PRIVATE
    benchmark::benchmark_main
    schengen_core
    sch::arrow
    sch::parquet
    ${_existing_arrow_priv_deps}
)
