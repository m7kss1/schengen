option(ENABLE_LANCE "Enable Lance format support" ${ENABLE_LIBRARIES})

if (NOT ENABLE_LANCE)
  message(STATUS "Not using Lance")
  return()
endif ()

if (NOT COMMAND corrosion_import_crate)
  message(FATAL_ERROR "Lance requires Corrosion")
endif ()

find_program(CARGO_EXECUTABLE cargo)
if (NOT CARGO_EXECUTABLE)
  message(FATAL_ERROR "Cargo is not found; cannot import Lance bridge")
endif ()

set(LANCE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lance")
set(LANCE_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lance-bridge")
set(LANCE_BRIDGE_MANIFEST "${LANCE_BRIDGE_DIR}/Cargo.toml")

if (NOT EXISTS "${LANCE_BRIDGE_MANIFEST}")
  message(FATAL_ERROR "Lance bridge manifest is missing: ${LANCE_BRIDGE_MANIFEST}")
endif ()
if (NOT EXISTS "${LANCE_ROOT_DIR}/Cargo.toml")
  message(FATAL_ERROR "Lance submodule is not initialized: ${LANCE_ROOT_DIR}")
endif ()

corrosion_import_crate(
  MANIFEST_PATH "${LANCE_BRIDGE_MANIFEST}"
  CRATES lance_bridge
)

if (NOT TARGET lance_bridge)
  message(FATAL_ERROR "Failed to resolve imported Lance Rust target from Corrosion")
endif ()

add_library(lance INTERFACE)
target_link_libraries(lance INTERFACE lance_bridge)
add_library(sch::lance ALIAS lance)

message(STATUS "Using Lance via Corrosion (crate target: lance_bridge)")
