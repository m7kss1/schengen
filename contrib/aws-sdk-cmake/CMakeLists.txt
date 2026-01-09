option(ENABLE_AWS_SDK "Enable AWS SDK for C++" ${ENABLE_LIBRARIES})

if (NOT ENABLE_AWS_SDK)
  message(STATUS "Not using AWS SDK for C++ (set ENABLE_AWS_SDK=ON to enable)")
  return()
endif ()

set(AWSSDK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../aws-sdk-cpp")

# Minimal AWS SDK build for Arrow S3 support.
set(BUILD_ONLY
    "config;core;identity-management;sts;s3;transfer"
    CACHE STRING "AWS SDK components to build" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_DEPS ON CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_PERFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_UNITY_BUILD ON CACHE BOOL "" FORCE)
set(MINIMIZE_SIZE OFF CACHE BOOL "" FORCE)
set(FORCE_CURL ON CACHE BOOL "" FORCE)
set(USE_OPENSSL ON CACHE BOOL "" FORCE)
set(CUSTOM_MEMORY_MANAGEMENT OFF CACHE BOOL "" FORCE)
set(AWS_SDK_WARNINGS_ARE_ERRORS OFF CACHE BOOL "" FORCE)
set(CPP_STANDARD 17 CACHE STRING "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

add_subdirectory(
  "${AWSSDK_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/aws-sdk-build"
)

# Help Arrow's FindAWSSDKAlt locate the AWS SDK CMake config.
set(AWSSDK_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/aws-sdk-build"
    CACHE PATH "AWS SDK CMake config path" FORCE)

if (TARGET aws-cpp-sdk-s3)
  add_library(sch::aws_sdk INTERFACE IMPORTED)
  target_link_libraries(sch::aws_sdk INTERFACE aws-cpp-sdk-s3 aws-cpp-sdk-transfer)
endif ()
