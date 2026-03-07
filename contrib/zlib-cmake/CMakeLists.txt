option(ENABLE_ZLIB "Enable bundled zlib" ${ENABLE_LIBRARIES})

if (NOT ENABLE_ZLIB)
  message(STATUS "Not using bundled zlib")
  return()
endif ()

set(ZLIB_SOURCE_DIR "${PROJECT_SOURCE_DIR}/contrib/zlib")

add_library(_zlib STATIC
  "${ZLIB_SOURCE_DIR}/adler32.c"
  "${ZLIB_SOURCE_DIR}/compress.c"
  "${ZLIB_SOURCE_DIR}/crc32.c"
  "${ZLIB_SOURCE_DIR}/deflate.c"
  "${ZLIB_SOURCE_DIR}/gzclose.c"
  "${ZLIB_SOURCE_DIR}/gzlib.c"
  "${ZLIB_SOURCE_DIR}/gzread.c"
  "${ZLIB_SOURCE_DIR}/gzwrite.c"
  "${ZLIB_SOURCE_DIR}/infback.c"
  "${ZLIB_SOURCE_DIR}/inffast.c"
  "${ZLIB_SOURCE_DIR}/inflate.c"
  "${ZLIB_SOURCE_DIR}/inftrees.c"
  "${ZLIB_SOURCE_DIR}/trees.c"
  "${ZLIB_SOURCE_DIR}/uncompr.c"
  "${ZLIB_SOURCE_DIR}/zutil.c"
)

target_include_directories(_zlib PUBLIC "${ZLIB_SOURCE_DIR}")
set_target_properties(_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ZLIB::ZLIB ALIAS _zlib)
