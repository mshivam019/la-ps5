#!/usr/bin/env bash
set -euo pipefail
# Private host tool prefix; does not change system packages.
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
version=2.16.03
archive="$root/.deps/src/nasm/nasm-$version.tar.gz"
source_dir="$root/.deps/src/nasm/nasm-$version"
prefix="$root/.deps/host/nasm"
mkdir -p "$(dirname "$archive")"
if [[ ! -f "$archive" ]]; then
    curl --fail --location --retry 3 "https://www.nasm.us/pub/nasm/releasebuilds/$version/nasm-$version.tar.gz" -o "$archive"
fi
printf '%s  %s\n' '5bc940dd8a4245686976a8f7e96ba9340a0915f2d5b88356874890e207bdb581' "$archive" | sha256sum --check --status
if [[ ! -f "$source_dir/configure" ]]; then
    tar -xzf "$archive" -C "$(dirname "$source_dir")"
fi
cd "$source_dir"
./configure --prefix="$prefix"
make -j4
make install
"$prefix/bin/nasm" -v
