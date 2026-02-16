set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL 1)

function(add_contrib cmake_folder)
  if (ARGN)
    set(base_folders ${ARGN})
  else()
    set(base_folders ${cmake_folder})
  endif()

  foreach (base_folder IN LISTS base_folders)
    if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${base_folder}")
      message(
        FATAL_ERROR
          "No such base folder '${base_folder}' (for '${cmake_folder}' cmake folder)."
      )
    endif()

    file(GLOB contrib_files "${base_folder}/*")
    if (NOT contrib_files)
      message(STATUS "submodule ${base_folder} is missing or empty. to fix run:")
      message(STATUS "    git submodule update --init --recursive")
      return()
    endif()
  endforeach()

  message(STATUS "Adding contrib module ${base_folders} (configuring with ${cmake_folder})")
  add_subdirectory(${cmake_folder})
endfunction()

add_contrib(boost-cmake boost)
add_contrib(thrift-cmake thrift)
add_contrib(zlib-cmake zlib)
add_contrib(snappy-cmake snappy)
add_contrib(rapidjson-cmake rapidjson)
add_contrib(aws-sdk-cmake aws-sdk-cpp)
add_contrib(arrow-cmake arrow)
add_contrib(googletest-cmake googletest)
add_contrib(benchmark-cmake benchmark)
add_contrib(yaclib-cmake yaclib)
