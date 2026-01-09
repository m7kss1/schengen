set(_awssdk_required_targets
    aws-cpp-sdk-core
    aws-cpp-sdk-s3
    aws-cpp-sdk-transfer
    aws-cpp-sdk-sts
    aws-cpp-sdk-identity-management)

set(_awssdk_missing FALSE)
foreach(_awssdk_target IN LISTS _awssdk_required_targets)
  if(NOT TARGET ${_awssdk_target})
    set(_awssdk_missing TRUE)
  endif()
endforeach()

if(_awssdk_missing)
  set(AWSSDK_FOUND FALSE)
  return()
endif()

get_target_property(_awssdk_include_dirs aws-cpp-sdk-core INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _awssdk_include_dirs)
  get_target_property(_awssdk_include_dirs aws-cpp-sdk-core INCLUDE_DIRECTORIES)
endif()

set(AWSSDK_FOUND TRUE)
set(AWSSDK_VERSION "1.11.0")
set(AWSSDK_INCLUDE_DIR "${_awssdk_include_dirs}")
set(AWSSDK_INCLUDE_DIRS "${_awssdk_include_dirs}")
set(AWSSDK_LIBRARIES ${_awssdk_required_targets})
set(AWSSDK_LINK_LIBRARIES ${_awssdk_required_targets})

mark_as_advanced(AWSSDK_INCLUDE_DIR AWSSDK_INCLUDE_DIRS AWSSDK_LIBRARIES AWSSDK_LINK_LIBRARIES)
