option(ENABLE_RAPIDJSON "Enable bundled RapidJSON" ${ENABLE_LIBRARIES})

if (NOT ENABLE_RAPIDJSON)
  message(STATUS "Not using bundled RapidJSON")
  return()
endif ()

set(RAPIDJSON_SOURCE_DIR "${PROJECT_SOURCE_DIR}/contrib/rapidjson/include")

add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON SYSTEM INTERFACE "${RAPIDJSON_SOURCE_DIR}")
