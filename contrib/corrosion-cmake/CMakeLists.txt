option(ENABLE_CORROSION "Enable Corrosion (Rust/CMake bridge)" ${ENABLE_LIBRARIES})

if (NOT ENABLE_CORROSION)
  message(STATUS "Not using Corrosion")
  return()
endif ()

set(CORROSION_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../corrosion")

if (NOT EXISTS "${CORROSION_SOURCE_DIR}/CMakeLists.txt")
  message(WARNING "Corrosion submodule is not initialized")
  return()
endif ()

add_subdirectory(
  "${CORROSION_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/corrosion-build"
)

message(STATUS "Using Corrosion from ${CORROSION_SOURCE_DIR}")
