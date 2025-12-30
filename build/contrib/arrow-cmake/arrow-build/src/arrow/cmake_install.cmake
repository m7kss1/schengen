# Install script for directory: /home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/util" TYPE FILE FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/util/config.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/release/libarrow.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES
    "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/ArrowConfig.cmake"
    "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/ArrowConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake"
         "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow/ArrowTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/CMakeFiles/Export/817832ed7b630b992c3753a0fa15add4/ArrowTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/Release/arrow.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow" TYPE FILE FILES
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/api.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/buffer.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/buffer_builder.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/builder.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/chunk_resolver.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/chunked_array.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/compare.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/config.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/datum.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/device.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/device_allocation_type_set.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/extension_type.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/memory_pool.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/memory_pool_test.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/pretty_print.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/record_batch.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/result.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/scalar.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/sparse_tensor.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/status.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/stl.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/stl_allocator.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/stl_iterator.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/table.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/table_builder.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/tensor.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/type.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/type_fwd.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/type_traits.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visit_array_inline.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visit_data_inline.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visit_scalar_inline.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visit_type_inline.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visitor.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/visitor_generate.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/ArrowOptions.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Arrow" TYPE FILE FILES "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/arrow-config.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/testing/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/array/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/compute/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/extension/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/io/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/tensor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/util/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/vendored/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
