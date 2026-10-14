#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
wraps=()
for name in fopen open stat lstat access mkdir rmdir unlink remove rename opendir realpath chdir getcwd; do
    wraps+=("-Wl,--wrap=$name")
done
mkdir -p "$root/build/host-tests"
"${CC:-cc}" -std=gnu11 -O1 -g -Wall -Wextra -Werror -pthread -fsanitize=undefined \
    "$root/ps5/platform/filesystem.c" "$root/ps5/tests/filesystem.c" "${wraps[@]}" \
    -o "$root/build/host-tests/filesystem"
"$root/build/host-tests/filesystem"
