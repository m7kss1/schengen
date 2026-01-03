option(ENABLE_GOOGLETEST "Enable GoogleTest" ${ENABLE_LIBRARIES})

if (NOT ENABLE_GOOGLETEST)
  message(STATUS "Not using GoogleTest")
  return()
endif ()

set(YATPCHGEN_GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../googletest")

set(BUILD_GMOCK OFF CACHE BOOL "Disable GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable GoogleTest install" FORCE)
set(gtest_build_samples OFF CACHE BOOL "Disable GoogleTest samples" FORCE)
set(gtest_build_tests OFF CACHE BOOL "Disable GoogleTest tests" FORCE)

add_subdirectory(
  "${YATPCHGEN_GOOGLETEST_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/googletest-build"
)

if (TARGET gtest)
  add_library(yatpchgen::gtest ALIAS gtest)
endif ()

if (TARGET gtest_main)
  add_library(yatpchgen::gtest_main ALIAS gtest_main)
endif ()
