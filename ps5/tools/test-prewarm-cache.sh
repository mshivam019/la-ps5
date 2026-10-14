#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=$(mktemp -d /tmp/la-prewarm-test.XXXXXX)
trap 'rm -rf "$OUT"' EXIT INT TERM
cc -std=c11 -O2 -Wall -Wextra -Werror -pthread -I"$ROOT/runtime/nvn/vulkan" \
  "$ROOT/ps5/tests/prewarm_cache_test.c" "$ROOT/runtime/nvn/vulkan/prewarm_cache.c" -o "$OUT/test"
"$OUT/test" "$OUT/cache.bin"
