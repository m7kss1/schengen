# Install script for directory: /home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/array" TYPE FILE FILES
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_base.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_binary.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_decimal.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_dict.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_nested.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_primitive.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/array_run_end.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_adaptive.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_base.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_binary.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_decimal.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_dict.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_nested.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_primitive.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_run_end.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_time.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/builder_union.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/concatenate.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/data.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/diff.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/statistics.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/util.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/array/validate.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/array/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
