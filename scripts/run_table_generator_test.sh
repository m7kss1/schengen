#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build --target table_generator_tests -- -j $(nproc)

# ./build/test/table_generator_tests --gtest_filter=${1}.*

ctest --test-dir build -R "(Region|Nation|Supplier|PartSupp|Part|Customer)GeneratorTest"