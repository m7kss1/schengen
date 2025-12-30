cmake_minimum_required(VERSION 3.25)

project(yatpchgen LANGUAGES CXX)

option(ENABLE_LIBRARIES "Enable building bundled third-party libraries" ON)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, ...)" FORCE)
endif ()

add_subdirectory(contrib)

if (TARGET yatpchgen::arrow AND TARGET yatpchgen::yaclib)
  add_executable(yatpchgen_main src/main.cpp)
  target_compile_features(yatpchgen_main PRIVATE cxx_std_17)
  target_link_libraries(
    yatpchgen_main
    PRIVATE
      yatpchgen::arrow
      yatpchgen::yaclib
  )
else ()
  if (NOT TARGET yatpchgen::arrow)
    message(STATUS "Apache Arrow is disabled, skipping yatpchgen_main target")
  endif ()
  if (NOT TARGET yatpchgen::yaclib)
    message(STATUS "YACLib is disabled, skipping yatpchgen_main target")
  endif ()
endif ()
