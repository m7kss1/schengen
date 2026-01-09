set(LIBRARY_DIR "${PROJECT_SOURCE_DIR}/contrib/boost")

# Minimal Boost build for Arrow/Parquet: headers + regex + filesystem + system.
# This mirrors ClickHouse's contrib/boost-cmake but keeps only what we need.

# headers-only
add_library(_boost_headers_only INTERFACE)
add_library(boost::headers_only ALIAS _boost_headers_only)
target_include_directories(_boost_headers_only SYSTEM BEFORE INTERFACE ${LIBRARY_DIR})
target_compile_definitions(
  _boost_headers_only
  INTERFACE
    BOOST_ASIO_STANDALONE=1
    BOOST_ASIO_HAS_STD_INVOKE_RESULT=1
)

if (NOT TARGET Boost::headers)
  add_library(Boost::headers ALIAS _boost_headers_only)
endif ()
if (NOT TARGET Boost::boost)
  add_library(Boost::boost ALIAS _boost_headers_only)
endif ()

# system
set(SRCS_SYSTEM
    "${LIBRARY_DIR}/libs/system/src/error_code.cpp")
add_library(_boost_system ${SRCS_SYSTEM})
add_library(boost::system ALIAS _boost_system)
target_include_directories(_boost_system PRIVATE ${LIBRARY_DIR})

# filesystem
set(SRCS_FILESYSTEM
    "${LIBRARY_DIR}/libs/filesystem/src/codecvt_error_category.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/directory.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/exception.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/operations.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/path.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/path_traits.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/portability.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/unique_path.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/utf8_codecvt_facet.cpp"
    "${LIBRARY_DIR}/libs/filesystem/src/windows_file_codecvt.cpp")
add_library(_boost_filesystem ${SRCS_FILESYSTEM})
add_library(boost::filesystem ALIAS _boost_filesystem)
target_include_directories(_boost_filesystem SYSTEM BEFORE PUBLIC ${LIBRARY_DIR})
target_link_libraries(_boost_filesystem PRIVATE _boost_system)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(_boost_filesystem PRIVATE BOOST_FILESYSTEM_HAS_POSIX_AT_APIS=1)
endif ()

# regex
set(SRCS_REGEX
    "${LIBRARY_DIR}/libs/regex/src/posix_api.cpp"
    "${LIBRARY_DIR}/libs/regex/src/regex_debug.cpp"
    "${LIBRARY_DIR}/libs/regex/src/regex.cpp"
    "${LIBRARY_DIR}/libs/regex/src/static_mutex.cpp"
    "${LIBRARY_DIR}/libs/regex/src/wide_posix_api.cpp")
add_library(_boost_regex ${SRCS_REGEX})
add_library(boost::regex ALIAS _boost_regex)
target_include_directories(_boost_regex PRIVATE ${LIBRARY_DIR})
