option(ENABLE_VORTEX "Enable Vortex format support" ${ENABLE_LIBRARIES})

if (NOT ENABLE_VORTEX)
  message(STATUS "Not using Vortex")
  return()
endif ()

if (NOT COMMAND corrosion_import_crate)
  message(FATAL_ERROR "Vortex requires Corrosion")
endif ()

find_program(CARGO_EXECUTABLE cargo)
if (NOT CARGO_EXECUTABLE)
  message(FATAL_ERROR "Cargo is not found; cannot import Vortex bridge")
endif ()

set(VORTEX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vortex")
set(VORTEX_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vortex-bridge")
set(VORTEX_BRIDGE_MANIFEST "${VORTEX_BRIDGE_DIR}/Cargo.toml")

if (NOT EXISTS "${VORTEX_BRIDGE_MANIFEST}")
  message(FATAL_ERROR "Vortex bridge manifest is missing: ${VORTEX_BRIDGE_MANIFEST}")
endif ()
if (NOT EXISTS "${VORTEX_ROOT_DIR}/Cargo.toml")
  message(FATAL_ERROR "Vortex submodule is not initialized: ${VORTEX_ROOT_DIR}")
endif ()

corrosion_import_crate(
  MANIFEST_PATH "${VORTEX_BRIDGE_MANIFEST}"
  CRATES vortex_bridge
)

if (NOT TARGET vortex_bridge)
  message(FATAL_ERROR "Failed to resolve imported Vortex Rust target from Corrosion")
endif ()

add_library(vortex INTERFACE)
target_link_libraries(vortex INTERFACE vortex_bridge)
add_library(sch::vortex ALIAS vortex)

message(STATUS "Using Vortex via Corrosion (crate target: vortex_bridge)")
