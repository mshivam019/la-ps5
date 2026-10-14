#!/usr/bin/env python3
"""Generate private portable-C game code from an already-extracted v1.0.0 dump."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUILD_ID = bytes.fromhex('ae16f71e002af8cb')


def inspect_nso(path):
    size = path.stat().st_size
    with path.open('rb') as stream:
        header = stream.read(0x100)
        if len(header) != 0x100 or header[:4] != b'NSO0':
            raise ValueError(f'Invalid NSO header: {path.name}')
        flags = struct.unpack_from('<I', header, 0xc)[0]
        for index in range(3):
            offset, memory_offset, length = struct.unpack_from('<III', header, 0x10 + index * 0x10)
            stored = struct.unpack_from('<I', header, 0x60 + index * 4)[0] if flags & (1 << index) else length
            if offset < 0x100 or offset + stored > size or memory_offset + length > 0x40000000:
                raise ValueError(f'NSO segment out of bounds: {path.name}')
    with path.open('rb') as stream:
        sha = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'name': path.name, 'bytes': size, 'build_id': header[0x40:0x60].hex(), 'sha256': sha}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exefs', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'generated')
    args = parser.parse_args()
    modules = [inspect_nso(args.exefs / name) for name in ('main', 'sdk')]
    if bytes.fromhex(modules[0]['build_id'])[:8] != BUILD_ID:
        raise ValueError("Link's Awakening v1.0.0 main build ID is required")
    if args.output.exists():
        raise ValueError('Choose a new output to preserve previous generated code')
    python = Path(sys.executable)
    try:
        __import__('capstone'); __import__('unicorn'); __import__('lz4.block')
    except ImportError as error:
        raise ValueError('The generator needs capstone, unicorn, and lz4 in its Python environment') from error
    for name, base in (('sdk', '0x7004000000'), ('main', '0x7000000000')):
        cmd = [str(python), str(ROOT / 'tools/gen.py'), name, str((args.exefs / name).resolve()),
               base, str((args.output / name).resolve()), '--simd', 'c']
        if name == 'sdk':
            cmd += ['--hle', str(ROOT / 'runtime/hle/hle_symbols.txt')]
        subprocess.run(cmd, cwd=ROOT / 'tools', check=True)
    receipt = {'platform': 'PS5 x86-64 target', 'simd': 'portable C', 'modules': modules,
               'generated_files': len(list(args.output.rglob('*.c')))}
    (args.output / 'generation-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
