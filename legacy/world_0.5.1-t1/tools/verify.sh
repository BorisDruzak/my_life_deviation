#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build-verify -DCMAKE_BUILD_TYPE=Release -DNPC_STRICT=ON
cmake --build build-verify --parallel "${JOBS:-2}"
ctest --test-dir build-verify --output-on-failure
./build-verify/world_sim validate --scenario game/scenarios/household.json
