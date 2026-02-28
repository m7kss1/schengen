#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-/workspace}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-verify-parquet-sf1}"
TEST_REGEX="${TEST_REGEX:-^.*GeneratorTest\\.MatchesReferenceTable$}"

if [[ ! -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
  echo "error: CMakeLists.txt not found in ${ROOT_DIR}" >&2
  echo "mount project into /workspace or pass project path as arg" >&2
  exit 2
fi

if [[ ! -f "${BUILD_DIR}/test/table_generator_tests" ]]; then
  echo "error: test binary is missing: ${BUILD_DIR}/test/table_generator_tests" >&2
  echo "run .docker/build-parquet-sf1.sh first" >&2
  exit 2
fi

ctest --test-dir "${BUILD_DIR}" \
      -R "${TEST_REGEX}" \
      --output-on-failure
