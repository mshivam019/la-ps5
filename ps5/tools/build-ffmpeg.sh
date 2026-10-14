#!/usr/bin/env bash
set -euo pipefail

# Build the small, LGPL-only FFmpeg subset used by runtime/hle/movie.c for the
# native PS5 ABI. Keep the source and build separate so rebuilding is repeatable.
readonly FFMPEG_VERSION=6.1.3
readonly FFMPEG_SHA256=bc5f1e4a4d283a6492354684ee1124129c52293bcfc6a9169193539fbece3487
readonly FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
source_dir="$root/.deps/src/ffmpeg"
build_dir="$root/build/ps5-ffmpeg"
install_dir="$build_dir/install"
archive="$source_dir/ffmpeg-${FFMPEG_VERSION}.tar.xz"

readonly sdk_default=/home/skm/ps5sdk/native-app-boilerplate/.deps/native/ps5-payload-sdk
readonly cc_default=/home/skm/ps5sdk/native-app-boilerplate/tooling/prospero-clang18
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$sdk_default}"
cc="${PS5_FFMPEG_CC:-$cc_default}"
export PATH="$root/.deps/host/nasm/bin:$PS5_PAYLOAD_SDK/bin:$PATH"

[[ -x "$cc" ]] || { echo "PS5 compiler wrapper not found: $cc" >&2; exit 2; }
[[ -d "$PS5_PAYLOAD_SDK/target/include" ]] || {
    echo "PS5_PAYLOAD_SDK is not usable: $PS5_PAYLOAD_SDK" >&2
    exit 2
}
if ! command -v nasm >/dev/null; then
    bash "$root/ps5/tools/build-nasm.sh"
fi
for tool in curl sha256sum tar make llvm-ar llvm-ranlib llvm-nm nasm; do
    command -v "$tool" >/dev/null || { echo "Required tool not found: $tool" >&2; exit 2; }
done

mkdir -p "$source_dir"
if [[ ! -x "$source_dir/configure" ]]; then
    if [[ ! -f "$archive" ]]; then
        curl --fail --location --retry 3 --output "$archive" "$FFMPEG_URL"
    fi
    printf '%s  %s\n' "$FFMPEG_SHA256" "$archive" | sha256sum --check --status || {
        echo "FFmpeg source archive checksum mismatch: $archive" >&2
        exit 1
    }
    tar -xJf "$archive" --strip-components=1 -C "$source_dir"
fi
[[ -f "$source_dir/RELEASE" && "$(<"$source_dir/RELEASE")" == "$FFMPEG_VERSION" ]] || {
    echo "Expected FFmpeg $FFMPEG_VERSION source in $source_dir" >&2
    exit 1
}
if [[ -f "$archive" ]]; then
    printf '%s  %s\n' "$FFMPEG_SHA256" "$archive" | sha256sum --check --status || {
        echo "FFmpeg source archive checksum mismatch: $archive" >&2
        exit 1
    }
fi

mkdir -p "$build_dir"
cd "$build_dir"
if [[ -f ffbuild/config.mak ]]; then
    make distclean
fi

"$source_dir/configure" \
    --prefix="$install_dir" \
    --enable-cross-compile \
    --target-os=freebsd \
    --arch=x86_64 \
    --cc="$cc" \
    --ld="$cc" \
    --ar=llvm-ar \
    --ranlib=llvm-ranlib \
    --nm=llvm-nm \
    --extra-cflags='-D__PROSPERO__ -fPIC -ffunction-sections -fdata-sections' \
    --extra-ldflags="-L$PS5_PAYLOAD_SDK/target/lib -lSceLibcInternal -lkernel -lpthread -lc" \
    --enable-pic \
    --enable-static \
    --disable-shared \
    --disable-autodetect \
    --disable-everything \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --disable-network \
    --disable-devices \
    --disable-avdevice \
    --disable-avfilter \
    --disable-postproc \
    --disable-swresample \
    --disable-encoders \
    --disable-muxers \
    --disable-hwaccels \
    --enable-x86asm \
    --disable-avx512 \
    --disable-avx512icl \
    --disable-symver \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swscale \
    --enable-pthreads \
    --enable-protocol=file \
    --enable-demuxer=matroska,mov \
    --enable-parser=aac,h264,vp8,vp9,vorbis \
    --enable-decoder=aac,h264,vp8,vp9,vorbis

make -j4
make install

for library in avformat avcodec avutil swscale; do
    test -s "$install_dir/lib/lib${library}.a"
done

echo "PS5 FFmpeg ${FFMPEG_VERSION} installed in $install_dir"
