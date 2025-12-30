# Install script for directory: /home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/util" TYPE FILE FILES
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/algorithm.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/align_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/aligned_storage.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/async_generator.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/async_generator_fwd.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/async_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/base64.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/basic_decimal.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/benchmark_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/binary_view_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bit_block_counter.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bit_run_reader.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bit_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_builders.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_generate.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_ops.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_reader.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_visit.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/bitmap_writer.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/byte_size.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/cancel.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/checked_cast.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/compare.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/compression.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/concurrent_map.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/converter.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/cpu_info.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/crc32.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/debug.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/decimal.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/delimiting.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/endian.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/float16.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/formatting.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/functional.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/future.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/hash_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/hashing.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/int_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/int_util_overflow.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/io_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/iterator.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/key_value_metadata.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/launder.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/list_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/logger.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/logging.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/macros.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/math_constants.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/mutex.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/parallel.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/pcg_random.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/prefetch.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/queue.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/range.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/ree_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/regex.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/rows_to_batches.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/secure_string.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/simd.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/small_vector.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/span.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/string.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/string_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/task_group.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/test_common.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/thread_pool.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/time.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/tracing.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/type_fwd.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/type_traits.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/ubsan.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/union_util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/unreachable.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/uri.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/utf8.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/value_parsing.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/vector.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/visibility.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/windows_compatibility.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/util/windows_fixup.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/util/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
