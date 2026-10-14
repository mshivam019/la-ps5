#!/usr/bin/env python3
"""Generate the ROM-free module ABI and the runtime import resolver."""
import argparse
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IMPORTS = ROOT / 'ps5/module/host-imports.txt'


def imports():
    names = [line.strip() for line in IMPORTS.read_text().splitlines() if line.strip() and not line.startswith('#')]
    if len(names) != len(set(names)) or not all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', name) for name in names):
        raise ValueError('Invalid or duplicate host import')
    return names


def abi():
    inputs = [ROOT / 'runtime/cpu.h', ROOT / 'runtime/simd.h', ROOT / 'runtime/rt.h',
              ROOT / 'runtime/hle.h', ROOT / 'runtime/rando.h', ROOT / 'tools/gen.py',
              ROOT / 'tools/patches.py', IMPORTS]
    return hashlib.sha256(b'links-awakening-ps5-module-v1\0' + b''.join(path.read_bytes() for path in inputs)).hexdigest()


def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    (output / 'abi.h').write_text(f'#define LA_MODULE_ABI "{abi()}"\n')
    lines = ['#include <cstring>']
    for index, name in enumerate(imports()):
        lines.append(f'extern "C" char la_host_{index}[] __asm__("{name}");')
    lines.append('void* la_host_symbol(const char* name) {')
    for index, name in enumerate(imports()):
        lines.append(f'    if (!strcmp(name, "{name}")) return la_host_{index};')
    lines += ['    return nullptr;', '}']
    (output / 'exports.cpp').write_text('\n'.join(lines) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    generate(args.output)
    print(f'Module ABI {abi()}; {len(imports())} runtime imports')


if __name__ == '__main__':
    main()
