#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build
${CC:-clang} -g -fsanitize=address,undefined ps5/tests/test-ipa.c runtime/nvn/maxwell/translate.c -o build/test-ipa
build/test-ipa
for variant in 0 1 2 3; do
    shader="build/test-ipa-$variant.frag"
    glslangValidator -V "$shader" -o "$shader.spv"
    spirv-val "$shader.spv"
done
