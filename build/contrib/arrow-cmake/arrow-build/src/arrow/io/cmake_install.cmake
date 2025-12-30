# Install script for directory: /home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/arrow/io" TYPE FILE FILES
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/api.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/buffered.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/caching.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/compressed.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/concurrency.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/file.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/hdfs.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/interfaces.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/memory.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/mman.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/slow.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/stdio.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/test_common.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/transform.h"
    "/home/ubuntu/yatpchgen/contrib/arrow/cpp/src/arrow/io/type_fwd.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/ubuntu/yatpchgen/build/contrib/arrow-cmake/arrow-build/src/arrow/io/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
