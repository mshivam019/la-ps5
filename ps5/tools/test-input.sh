#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
mkdir -p "$root/build/host-tests"
c++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined \
    "$root/ps5/platform/input.cpp" "$root/ps5/tests/input.cpp" \
    $(pkg-config --cflags --libs sdl3) -o "$root/build/host-tests/input"
SDL_JOYSTICK_HIDAPI=0 "$root/build/host-tests/input"
