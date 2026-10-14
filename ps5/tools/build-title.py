#!/usr/bin/env python3
"""Build the private native title using Mihawk's RADV/template link recipe."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parent
SDK = ROOT / '.deps/native/ps5-payload-sdk'


def run(*args, **kwargs):
    subprocess.run(list(map(str, args)), check=True, **kwargs)


def copy_changed(source, destination):
    if destination.is_file() and source.read_bytes() == destination.read_bytes():
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def prepare_sdk():
    base = Path.home() / 'ps5sdk/native-app-boilerplate/.deps/native/ps5-payload-sdk'
    if not SDK.exists():
        shutil.copytree(base, SDK, symlinks=True)
    platform = WORKSPACE / 'PS5_PayloadSDK/platform'
    for source in (platform / 'include').rglob('*.h'):
        copy_changed(source, SDK / 'target/include' / source.relative_to(platform / 'include'))
    copy_changed(platform / 'build/ps5/libps5platform.a', SDK / 'target/lib/libps5platform.a')
    copy_changed(WORKSPACE / 'PS5_PayloadSDK/include/freebsd/sys/_ucontext.h', SDK / 'target/include/sys/_ucontext.h')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dependencies', action='store_true', help='Build shaderc and FFmpeg first')
    parser.add_argument('--clean', action='store_true', help='Rebuild all title objects with the prepared SDK')
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.dependencies:
        run(sys.executable, ROOT / 'ps5/tools/build-shaderc.py')
        run('bash', ROOT / 'ps5/tools/build-ffmpeg.sh')
    prepare_sdk()
    env = os.environ.copy()
    env['PS5_PAYLOAD_SDK'] = str(SDK)
    env['LA_BUILD_JOBS'] = str(args.jobs)
    # Configure first so --clean cannot accidentally reuse the older SDK.
    run('cmake', '-S', ROOT / 'ps5', '-B', ROOT / 'build/ps5-core', '-G', 'Ninja',
        f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/ps5/cmake/native.cmake',
        f'-DPS5_PAYLOAD_SDK={SDK}', '-DLA_COMPILE_VULKAN=ON', '-DLA_COMPILE_UI=ON',
        '-DCMAKE_BUILD_TYPE=Release', env=env)
    command = ['cmake', '--build', ROOT / 'build/ps5-core', '--parallel', args.jobs]
    if args.clean:
        command.append('--clean-first')
    run(*command, env=env)
    core = ROOT / 'build/ps5-core'
    shaderc = ROOT / 'build/ps5-shaderc/libshaderc/libshaderc_combined.a'
    ffmpeg = ROOT / 'build/ps5-ffmpeg/install/lib'
    sdl = WORKSPACE / 'Wind-Waker-HD-PS5/build/sdl3-ps5/libSDL3.a'
    archives = [core / f'libla_ps5_{name}.a' for name in ('platform', 'entry', 'core', 'memory', 'ui', 'vulkan', 'module')]
    dependencies = [shaderc,
                    *(ffmpeg / f'lib{name}.a' for name in ('avformat', 'avcodec', 'swscale', 'avutil')), sdl]
    for path in archives + dependencies:
        if not path.is_file():
            raise ValueError(f'Missing native library: {path}')
    vulkan = WORKSPACE / 'PS5_Vulkan'
    expected = (vulkan / 'runtime/libc.prx.sha256').read_text().split()[0]
    with (vulkan / 'runtime/libc.prx').open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
            raise ValueError('Native libc.prx digest mismatch')
    run('bash', ROOT / 'ps5/tools/link-title.sh', ROOT / 'build/native-title', vulkan, SDK,
        '--whole-archive', *archives, '--no-whole-archive', '--start-group', *dependencies, '--end-group', env=env)
    run(sys.executable, ROOT / 'ps5/tools/validate.py', '--title')
    print('ROM-free native executable:', ROOT / 'build/native-title/eboot.bin')
    print('Console execution remains unverified. Supply a matching private game module to play.')


if __name__ == '__main__':
    main()
