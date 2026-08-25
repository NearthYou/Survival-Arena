#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

apt-get update
apt-get install --yes \
  build-essential \
  ca-certificates \
  cmake \
  curl \
  git \
  ninja-build \
  pkg-config \
  tar \
  unzip \
  zip

vcpkg_root=/build/vcpkg
build_root=/build/dxa-linux-asan
if [[ ! -d "$vcpkg_root/.git" ]]; then
  git init "$vcpkg_root"
  git -C "$vcpkg_root" remote add origin https://github.com/microsoft/vcpkg.git
  git -C "$vcpkg_root" fetch --depth 1 origin 127402f1c75bb3d5ff6bce04b285faa4930a5aca
  git -C "$vcpkg_root" checkout --detach FETCH_HEAD
fi
"$vcpkg_root/bootstrap-vcpkg.sh" -disableMetrics

cmake \
  -S /workspace \
  -B "$build_root" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DDXA_BUILD_TESTS=ON \
  -DDXA_WARNINGS_AS_ERRORS=ON \
  2>&1 | tee /evidence/linux-asan-build.log
cmake --build "$build_root" 2>&1 | tee -a /evidence/linux-asan-build.log

started_epoch=$(date +%s)
deadline_epoch=$((started_epoch + 1800))
runs=0
: > /evidence/linux-asan.log
while (( $(date +%s) < deadline_epoch )); do
  timeout 30s \
    "$build_root/tests/dxa_tests" \
    --gtest_filter=GameServerIntegration.PlayCoordinatorReportsEverySession \
    --gtest_brief=1 \
    2>&1 | tee -a /evidence/linux-asan.log
  runs=$((runs + 1))
done
finished_epoch=$(date +%s)
duration_seconds=$((finished_epoch - started_epoch))
printf '{"schema_version":1,"runs":%d,"duration_seconds":%d,"exit_code":0}\n' \
  "$runs" \
  "$duration_seconds" \
  > /evidence/linux-asan-summary.json
