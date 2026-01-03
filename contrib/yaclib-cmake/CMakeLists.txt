option(ENABLE_YACLIB "Enable YACLib" ${ENABLE_LIBRARIES})

if (NOT ENABLE_YACLIB)
  message(STATUS "Not using YACLib")
  return()
endif ()

set(YACLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../yaclib")

set(YACLIB_TEST OFF CACHE STRING "Disable upstream YACLib tests" FORCE)
set(YACLIB_INSTALL OFF CACHE BOOL "Disable upstream YACLib install targets" FORCE)

add_subdirectory(
  "${YACLIB_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/yaclib-build"
)

set(YACLIB_INCLUDE_DIRS
    "${YACLIB_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/yaclib-build/include"
    CACHE INTERNAL "YACLib include directories" FORCE)

if (TARGET yaclib)
  add_library(sch::yaclib ALIAS yaclib)
endif ()
