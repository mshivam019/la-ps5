#!/usr/bin/env python3
"""Linux setup: extract an owned v1.0.0 base NSP and build its private PS5 game module."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(*command):
    log = ROOT / 'build/setup.log'
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('a', encoding='utf-8') as stream:
        subprocess.run(list(map(str, command)), check=True, stdout=stream, stderr=subprocess.STDOUT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nsp', required=True, type=Path,
                        help='Owned Link’s Awakening v1.0.0 base-game NSP (not an update)')
    parser.add_argument('--keys', required=True, type=Path, help='Your local prod.keys')
    parser.add_argument('--output', type=Path, default=ROOT.parent / 'Links-Awakening-PS5-private/PPSA99670')
    parser.add_argument('--jobs', type=int, default=max(1, min(6, os.cpu_count() or 1)))
    parser.add_argument('--clang', default=shutil.which('clang'))
    parser.add_argument('--linker', default=shutil.which('ld.lld'))
    parser.add_argument('--ar', default=shutil.which('llvm-ar'))
    parser.add_argument('--nm', default=shutil.which('llvm-nm'))
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('This setup workflow is currently tested only on Linux x86-64; Windows and macOS are not claimed')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if not args.nsp.is_file() or not args.keys.is_file():
        parser.error('--nsp and --keys must both be readable local files')
    for name in ('clang', 'linker', 'ar', 'nm'):
        if not getattr(args, name):
            parser.error(f'Missing local LLVM tool: {name}')
    venv = ROOT / 'build/setup-python'
    python = venv / 'bin/python'
    if not python.is_file():
        print('Preparing the local private-code generator...', flush=True)
        run(sys.executable, '-m', 'venv', venv)
        run(python, '-m', 'pip', 'install', '--disable-pip-version-check',
            'capstone==5.0.9', 'unicorn==2.1.4', 'lz4==4.4.5')
    bundled_hactool = ROOT / 'host-tools/linux-x86_64/hactool'
    source_hactool = ROOT / 'tools/hactool/hactool'
    hactool = bundled_hactool if bundled_hactool.is_file() else source_hactool
    if not hactool.is_file():
        parser.error('Missing local hactool; build the pinned setup dependency first')
    game = ROOT / 'build/private-extracted'
    if game.exists():
        parser.error(f'Private extraction output already exists: {game}; move it aside before a new setup')
    print('Extracting your local base-game dump...', flush=True)
    run(sys.executable, ROOT / 'ps5/tools/extract-game.py', '--nsp', args.nsp.resolve(),
        '--keys', args.keys.resolve(), '--hactool', hactool, '--output', game)
    generated = ROOT / 'build/private-generated'
    receipt = generated / 'generation-receipt.json'
    regenerate = not receipt.is_file()
    if not regenerate:
        recorded = json.loads(receipt.read_text())
        regenerate = any(sha(game / 'exefs' / row['name']) != row['sha256'] for row in recorded['modules'])
    if regenerate:
        if generated.exists():
            parser.error('Generated cache belongs to another dump; move build/private-generated aside')
        print('Generating private game code from your dump...', flush=True)
        run(python, ROOT / 'ps5/tools/generate-game.py', '--exefs', game / 'exefs', '--output', generated)
    work = ROOT / 'build/private-game-module'
    objects = work / 'objects'
    objects.mkdir(parents=True, exist_ok=True)
    flags = ['-fPIC', '-O1', '-march=x86-64-v3', '-DNDEBUG', '-std=gnu11', '-fno-stack-protector',
             '-fno-strict-aliasing', '-Wno-return-type', '-Wno-unused-label', '-I' + str(ROOT / 'runtime')]
    sources = sorted((generated / 'main').glob('*.c')) + sorted((generated / 'sdk').glob('*.c'))
    build_identity = hashlib.sha256((str(flags) + sha(ROOT / 'runtime/cpu.h') + sha(ROOT / 'runtime/simd.h')).encode()).hexdigest()

    def compile_one(source_path):
        obj = objects / (source_path.parent.name + '-' + source_path.stem + '.o')
        stamp = obj.with_suffix('.sha256')
        identity = hashlib.sha256((build_identity + sha(source_path)).encode()).hexdigest()
        if not (obj.is_file() and stamp.is_file() and stamp.read_text() == identity + ':' + sha(obj)):
            run(args.clang, *flags, '-c', source_path, '-o', obj)
            stamp.write_text(identity + ':' + sha(obj))
        return obj

    print(f'Compiling {len(sources)} private generated units with {args.jobs} jobs...', flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        compiled = list(executor.map(compile_one, sources))
    archive = work / 'libla-game.a'
    temporary = work / 'libla-game.a.new'
    temporary.unlink(missing_ok=True)
    run(args.ar, 'rcs', temporary, *compiled)
    temporary.replace(archive)
    module = work / 'game.la'
    run(sys.executable, ROOT / 'ps5/tools/build-game-module.py', '--archive', archive,
        '--clang', args.clang, '--linker', args.linker, '--nm', args.nm, '--output', module)
    run(sys.executable, ROOT / 'ps5/tools/package-game.py', '--game', game, '--module', module, '--output', args.output)
    print(f'\nReady: {args.output.resolve()}\nThis private output contains game data. Do not redistribute it.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'Setup failed: {error}\nDetails: {ROOT / "build/setup.log"}', file=sys.stderr)
        sys.exit(1)
