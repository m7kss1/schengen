option(ENABLE_YACLIB "Enable YACLib" ${ENABLE_LIBRARIES})

if (NOT ENABLE_YACLIB)
  message(STATUS "Not using YACLib")
  return()
endif ()

set(YATPCHGEN_YACLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../yaclib")

set(YACLIB_TEST OFF CACHE STRING "Disable upstream YACLib tests" FORCE)
set(YACLIB_INSTALL OFF CACHE BOOL "Disable upstream YACLib install targets" FORCE)

add_subdirectory(
  "${YATPCHGEN_YACLIB_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/yaclib-build"
)

if (TARGET yaclib)
  add_library(yatpchgen::yaclib ALIAS yaclib)
endif ()
