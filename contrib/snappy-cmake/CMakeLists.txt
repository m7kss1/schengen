option(ENABLE_SNAPPY "Enable bundled snappy" ${ENABLE_LIBRARIES})

if (NOT ENABLE_SNAPPY)
  message(STATUS "Not using bundled snappy")
  return()
endif ()

set(SNAPPY_SOURCE_DIR "${PROJECT_SOURCE_DIR}/contrib/snappy")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

set(SNAPPY_SOURCES
    "${SNAPPY_SOURCE_DIR}/snappy.cc"
    "${SNAPPY_SOURCE_DIR}/snappy-c.cc"
    "${SNAPPY_SOURCE_DIR}/snappy-sinksource.cc"
    "${SNAPPY_SOURCE_DIR}/snappy-stubs-internal.cc"
)

set(HAVE_BYTESWAP_H 1)
set(HAVE_SYS_MMAN_H 1)
set(HAVE_SYS_RESOURCE_H 1)
set(HAVE_SYS_TIME_H 1)
set(HAVE_SYS_UIO_H 1)
set(HAVE_UNISTD_H 1)

set(HAVE_BUILTIN_EXPECT 1)
set(HAVE_BUILTIN_CTZ 1)
set(HAVE_FUNC_MMAP 1)
set(HAVE_FUNC_SYSCONF 1)

set(HAVE_SYS_UIO_H_01 ${HAVE_SYS_UIO_H})
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 2)

configure_file(
  "${SNAPPY_SOURCE_DIR}/cmake/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

configure_file(
  "${SNAPPY_SOURCE_DIR}/snappy-stubs-public.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/snappy-stubs-public.h"
)

add_library(snappy STATIC ${SNAPPY_SOURCES})
target_include_directories(snappy PUBLIC
  "${SNAPPY_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(snappy PRIVATE HAVE_CONFIG_H)
target_compile_features(snappy PUBLIC cxx_std_11)
set_target_properties(snappy PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(Snappy::snappy ALIAS snappy)
add_library(Snappy::snappy-static ALIAS snappy)
add_library(SNAPPY::SNAPPY ALIAS snappy)
