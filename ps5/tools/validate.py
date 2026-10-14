#!/usr/bin/env python3
"""Validate the native compilation milestone and private-source exclusion."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--with-game', action='store_true', help='Require the separate private game module too')
    parser.add_argument('--title', action='store_true', help='Require and record the signed native executable')
    args = parser.parse_args()
    cache = (ROOT / 'build/ps5-core/CMakeCache.txt').read_text()
    descriptions = list((ROOT / 'build/ps5-core/CMakeFiles').glob('*/CMakeCCompiler.cmake'))
    if len(descriptions) != 1:
        raise ValueError('Expected one configured compiler description')
    match = re.search(r'set\(CMAKE_C_COMPILER "([^"]+)"\)', descriptions[0].read_text())
    if not match or 'x86_64-sie-ps5' not in Path(match[1]).read_text() or '-D__PROSPERO__' not in cache:
        raise ValueError('Build does not use the recorded native PS5 compiler')
    tracked = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '-z']).decode().split('\0')
    for name in tracked:
        path = Path(name)
        if path.suffix.lower() in {'.nsp', '.xci', '.nca', '.nso', '.keys', '.tik', '.cert'} or any(
                part in {'generated', 'extracted', 'input', '.deps', 'exefs', 'romfs'} for part in path.parts):
            raise ValueError(f'Private game/tool content tracked: {name}')
    archives = ['libla_ps5_core.a', 'libla_ps5_vulkan.a', 'libla_ps5_memory.a', 'libla_ps5_entry.a',
                'libla_ps5_ui.a', 'libla_ps5_platform.a', 'libla_ps5_module.a']
    results = []
    for name in archives:
        path = ROOT / 'build/ps5-core' / name
        members = subprocess.check_output(['llvm-ar', 't', str(path)], text=True).splitlines()
        if not members:
            raise ValueError(f'Empty native archive: {name}')
        with path.open('rb') as stream:
            sha = hashlib.file_digest(stream, 'sha256').hexdigest()
        results.append({'archive': name, 'objects': len(members), 'bytes': path.stat().st_size, 'sha256': sha})
    object_count = 0
    for path in (ROOT / 'build/ps5-core/CMakeFiles').rglob('*.o'):
        with path.open('rb') as stream:
            header = stream.read(64)
        if header[:4] != b'\x7fELF' or header[4] != 2 or struct.unpack_from('<HH', header, 16) != (1, 62):
            raise ValueError(f'Unexpected architecture/type: {path.name}')
        object_count += 1
    # PS5 is ELF. A successful compile alone misses accidental Windows section
    # selection, which silently leaves import/reset registries empty at runtime.
    for archive, names in [('libla_ps5_core.a', ['hle_impls', 'services']), ('libla_ps5_entry.a', ['resets'])]:
        symbols = subprocess.check_output(['llvm-nm', '--undefined-only',
            str(ROOT / 'build/ps5-core' / archive)], text=True)
        for name in names:
            for prefix in ['__start_', '__stop_']:
                if not re.search(r'\bU ' + prefix + name + r'$', symbols, re.M):
                    raise ValueError(f'Missing ELF registry boundary: {archive}: {prefix}{name}')
    sections = subprocess.check_output(['readelf', '-SW',
        str(ROOT / 'build/ps5-core/libla_ps5_core.a')], text=True)
    if '.hle$' in sections or '.rst$' in sections or '.svc$' in sections or not re.search(r'\bresets\b', sections) or 'hle_impls' not in sections:
        raise ValueError('Native runtime registration sections are missing or use PE/COFF names')
    title = None
    if args.title:
        title = {}
        for name in ('eboot.elf', 'eboot.bin'):
            path = ROOT / 'build/native-title' / name
            with path.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            if path.stat().st_size < 1024:
                raise ValueError(f'Empty native title: {name}')
            title[name] = {'bytes': path.stat().st_size, 'sha256': digest}
        symbols = subprocess.check_output(['llvm-nm', str(ROOT / 'build/native-title/link/llvm-pie.elf')], text=True)
        if re.search(r'^[0-9a-f]+ [A-Za-z] f_[0-9a-f]{10,}$', symbols, re.M):
            raise ValueError('Public title embeds generated game functions')
        forbidden = ('main_fn_addrs', 'main_fn_ptrs', 'sdk_fn_addrs', 'sdk_fn_ptrs')
        for name in forbidden:
            if re.search(r'^[0-9a-f]+ [A-Za-z] ' + name + r'$', symbols, re.M):
                raise ValueError(f'Public title embeds a generated game table: {name}')
        for name in ('hle_impls', 'resets', 'services'):
            bounds = []
            for prefix in ('__start_', '__stop_'):
                match = re.search(r'^([0-9a-f]+) [a-zA-Z] ' + prefix + name + r'$', symbols, re.M)
                if not match:
                    raise ValueError(f'Missing linked registry: {prefix}{name}')
                bounds.append(int(match[1], 16))
            if bounds[1] <= bounds[0]:
                raise ValueError(f'Empty linked registry: {name}')
    private_module = None
    if args.with_game:
        path = ROOT / 'build/private-game-module/game.la'
        if not path.is_file():
            raise ValueError('Missing separate private game module: build/private-game-module/game.la')
        dynamic = subprocess.check_output(['llvm-nm', '-D', str(path)], text=True)
        for name in ('la_game_abi', 'main_fn_addrs', 'main_fn_ptrs', 'main_fn_count',
                     'sdk_fn_addrs', 'sdk_fn_ptrs', 'sdk_fn_count'):
            if not re.search(r'\b[A-Za-z] ' + name + r'$', dynamic, re.M):
                raise ValueError(f'Private module is missing export: {name}')
        with path.open('rb') as stream:
            private_module = {'path': str(path.relative_to(ROOT)), 'bytes': path.stat().st_size,
                              'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}
    print(json.dumps({'platform': 'PS5 compiler, x86-64 objects', 'archives': results,
        'elf_registration_sections': True,
        'checked_object_headers': object_count, 'linked_title': title, 'private_game_module': private_module,
        'rom_free_title': True, 'console_tested': False}, indent=2))


if __name__ == '__main__':
    main()
