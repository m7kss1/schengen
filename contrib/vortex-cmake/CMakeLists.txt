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
  message(WARNING "Cargo is not found; skipping Vortex import")
  return()
endif ()
find_program(CXXBRIDGE_EXECUTABLE cxxbridge)
if (NOT CXXBRIDGE_EXECUTABLE)
  message(WARNING "Cxxbridge is not found; skipping Vortex import")
  return()
endif ()

set(VORTEX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vortex")
set(VORTEX_CXX_DIR "${VORTEX_ROOT_DIR}/vortex-cxx")
set(VORTEX_INCLUDE_DIR "${VORTEX_CXX_DIR}/cpp/include")
set(VORTEX_CXX_MANIFEST "${VORTEX_CXX_DIR}/Cargo.toml")
set(VORTEX_CXX_BRIDGE_RS "${VORTEX_CXX_DIR}/src/lib.rs")
set(VORTEX_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VORTEX_GENERATED_BRIDGE_HEADER "${VORTEX_GENERATED_INCLUDE_DIR}/vortex_cxx_bridge/lib.h")
set(VORTEX_GENERATED_BRIDGE_SOURCE "${VORTEX_GENERATED_INCLUDE_DIR}/vortex_cxx_bridge/lib.cc")
set(VORTEX_GENERATED_CXX_HEADER "${VORTEX_GENERATED_INCLUDE_DIR}/rust/cxx.h")
set(VORTEX_GENERATED_NANOARROW_INLINE_TYPES "${VORTEX_GENERATED_INCLUDE_DIR}/nanoarrow/common/inline_types.h")

if (NOT EXISTS "${VORTEX_CXX_MANIFEST}")
  message(WARNING "Vortex submodule is not initialized")
  return()
endif ()

if (NOT IS_DIRECTORY "${VORTEX_INCLUDE_DIR}")
  message(FATAL_ERROR "Vortex include directory is missing: ${VORTEX_INCLUDE_DIR}")
endif ()
if (NOT EXISTS "${VORTEX_CXX_BRIDGE_RS}")
  message(FATAL_ERROR "Vortex CXX bridge source is missing: ${VORTEX_CXX_BRIDGE_RS}")
endif ()

file(MAKE_DIRECTORY "${VORTEX_GENERATED_INCLUDE_DIR}/vortex_cxx_bridge")
file(MAKE_DIRECTORY "${VORTEX_GENERATED_INCLUDE_DIR}/rust")
file(MAKE_DIRECTORY "${VORTEX_GENERATED_INCLUDE_DIR}/nanoarrow/common")
file(WRITE "${VORTEX_GENERATED_NANOARROW_INLINE_TYPES}" "#pragma once\n#include <arrow/c/abi.h>\n")
execute_process(
  COMMAND "${CXXBRIDGE_EXECUTABLE}" "${VORTEX_CXX_BRIDGE_RS}" --header
  OUTPUT_FILE "${VORTEX_GENERATED_BRIDGE_HEADER}"
  RESULT_VARIABLE VORTEX_CXXBRIDGE_EXIT_CODE
)
if (NOT VORTEX_CXXBRIDGE_EXIT_CODE EQUAL 0)
  message(FATAL_ERROR "Failed to generate Vortex CXX bridge header with cxxbridge")
endif ()
# Rust side exports symbols with the cxx bridge ABI version suffix (194).
file(READ "${VORTEX_GENERATED_BRIDGE_HEADER}" VORTEX_CXX_BRIDGE_HEADER_CONTENTS)
string(REPLACE "vortex$ffi$cxxbridge1$" "vortex$ffi$cxxbridge1$194$" VORTEX_CXX_BRIDGE_HEADER_CONTENTS "${VORTEX_CXX_BRIDGE_HEADER_CONTENTS}")
file(WRITE "${VORTEX_GENERATED_BRIDGE_HEADER}" "${VORTEX_CXX_BRIDGE_HEADER_CONTENTS}")
execute_process(
  COMMAND "${CXXBRIDGE_EXECUTABLE}" "${VORTEX_CXX_BRIDGE_RS}"
  OUTPUT_FILE "${VORTEX_GENERATED_BRIDGE_SOURCE}"
  RESULT_VARIABLE VORTEX_CXXBRIDGE_CC_EXIT_CODE
)
if (NOT VORTEX_CXXBRIDGE_CC_EXIT_CODE EQUAL 0)
  message(FATAL_ERROR "Failed to generate Vortex CXX bridge source with cxxbridge")
endif ()
file(READ "${VORTEX_GENERATED_BRIDGE_SOURCE}" VORTEX_CXX_BRIDGE_SOURCE_CONTENTS)
string(REPLACE "vortex$ffi$cxxbridge1$" "vortex$ffi$cxxbridge1$194$" VORTEX_CXX_BRIDGE_SOURCE_CONTENTS "${VORTEX_CXX_BRIDGE_SOURCE_CONTENTS}")
file(WRITE "${VORTEX_GENERATED_BRIDGE_SOURCE}" "${VORTEX_CXX_BRIDGE_SOURCE_CONTENTS}")
execute_process(
  COMMAND "${CXXBRIDGE_EXECUTABLE}" --header
  OUTPUT_FILE "${VORTEX_GENERATED_CXX_HEADER}"
  RESULT_VARIABLE VORTEX_CXXBRIDGE_RUNTIME_EXIT_CODE
)
if (NOT VORTEX_CXXBRIDGE_RUNTIME_EXIT_CODE EQUAL 0)
  message(FATAL_ERROR "Failed to generate rust/cxx.h with cxxbridge")
endif ()

corrosion_import_crate(
  MANIFEST_PATH "${VORTEX_CXX_MANIFEST}"
  CRATES vortex-cxx
)

set(_vortex_rust_target "")
foreach(_candidate IN ITEMS vortex_cxx vortex-cxx)
  if (TARGET ${_candidate})
    set(_vortex_rust_target ${_candidate})
    break()
  endif ()
endforeach()

if (_vortex_rust_target STREQUAL "")
  message(FATAL_ERROR "Failed to resolve imported Vortex Rust target from Corrosion")
endif ()

add_library(vortex_cxx_bridge STATIC
  "${VORTEX_CXX_DIR}/cpp/src/write_options.cpp"
  "${VORTEX_GENERATED_BRIDGE_SOURCE}"
)
target_compile_features(vortex_cxx_bridge PUBLIC cxx_std_17)
target_include_directories(vortex_cxx_bridge SYSTEM
  PUBLIC
    "${VORTEX_INCLUDE_DIR}"
    "${VORTEX_GENERATED_INCLUDE_DIR}"
)
if (DEFINED ARROW_INCLUDE_DIRS)
  target_include_directories(vortex_cxx_bridge SYSTEM PUBLIC ${ARROW_INCLUDE_DIRS})
endif ()
target_link_libraries(vortex_cxx_bridge PUBLIC ${_vortex_rust_target})

add_library(vortex INTERFACE)
target_include_directories(vortex SYSTEM INTERFACE
  "${VORTEX_INCLUDE_DIR}"
  "${VORTEX_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(vortex INTERFACE vortex_cxx_bridge)
add_library(sch::vortex ALIAS vortex)

set(VORTEX_INCLUDE_DIRS "${VORTEX_INCLUDE_DIR}" CACHE INTERNAL "Vortex include directories" FORCE)
message(STATUS "Using Vortex via Corrosion (crate target: ${_vortex_rust_target})")
