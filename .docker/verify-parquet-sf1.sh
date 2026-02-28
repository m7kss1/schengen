#!/usr/bin/env bash
set -euo pipefail

cleanup() {
  local dir="${BUILD_DIR:-}"
  if [[ -n "${dir}" && "${dir}" != "/" && -d "${dir}" ]]; then
    rm -rf "${dir}"
  fi
}
trap cleanup EXIT

ROOT_DIR="${1:-/workspace}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-verify-parquet-sf1}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"
TEST_REGEX="${TEST_REGEX:-^.*GeneratorTest\\.MatchesReferenceTable$}"

if [[ ! -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
  echo "error: CMakeLists.txt not found in ${ROOT_DIR}" >&2
  echo "mount project into /workspace or pass project path as arg" >&2
  exit 2
fi

cmake -S "${ROOT_DIR}" \
      -B "${BUILD_DIR}" \
      -G "${GENERATOR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DUSE_SYSTEM_ARROW=ON \
      -DENABLE_FORMATS=OFF \
      -DENABLE_PARQUET=ON \
      -DENABLE_GOOGLETEST=ON \
      -DENABLE_BENCHMARK=OFF

cmake --build "${BUILD_DIR}" --target table_generator_tests -j"$(nproc)"

ctest --test-dir "${BUILD_DIR}" \
      -R "${TEST_REGEX}" \
      --output-on-failure
