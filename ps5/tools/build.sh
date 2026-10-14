#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-${PS5SDK_ROOT:-$HOME/ps5sdk}/native-app-boilerplate/.deps/native/ps5-payload-sdk}"
cmake -S "$root/ps5" -B "$root/build/ps5-core" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/ps5/cmake/native.cmake" -DCMAKE_BUILD_TYPE=Release "$@"
PS5_PAYLOAD_SDK=$(sed -n 's/^PS5_PAYLOAD_SDK:PATH=//p' "$root/build/ps5-core/CMakeCache.txt")
export PS5_PAYLOAD_SDK
[[ -d "$PS5_PAYLOAD_SDK/target/include" ]] || { echo "Configured SDK is missing" >&2; exit 2; }
cmake --build "$root/build/ps5-core" --parallel "${LA_BUILD_JOBS:-6}"
