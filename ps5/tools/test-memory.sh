#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
platform=${PS5_PLATFORM_SOURCE:-$root/../PS5_PayloadSDK/platform}
mkdir -p "$root/build/host-tests"
"${CC:-cc}" -std=gnu11 -O1 -g -Wall -Wextra -Werror -pthread \
    -I"$root/runtime" -I"$platform/include" -I"$platform/tests" \
    "$root/ps5/platform/guest_memory.c" "$root/ps5/tests/guest_memory.c" \
    "$platform/src/shm.c" "$platform/src/placement.c" "$platform/tests/host_kernel.c" \
    -o "$root/build/host-tests/guest-memory"
"$root/build/host-tests/guest-memory"
