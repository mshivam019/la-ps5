#!/usr/bin/env python3
"""Privately extract the user's base NSP, matching upstream's hactool workflow."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_BUILD = bytes.fromhex('ae16f71e002af8cb')


def entries(stream, total):
    header = stream.read(16)
    if len(header) != 16 or header[:4] != b'PFS0':
        raise ValueError('Expected an NSP/PFS0 base-game image')
    count, string_size = struct.unpack_from('<II', header, 4)
    if not 0 < count <= 1024 or string_size > 0x100000:
        raise ValueError('Invalid NSP table dimensions')
    base = 16 + count * 24 + string_size
    if base > total:
        raise ValueError('NSP table out of bounds')
    table = stream.read(count * 24)
    strings = stream.read(string_size)
    result = []
    for index in range(count):
        offset, size, name_offset = struct.unpack_from('<QQI', table, index * 24)
        if name_offset >= len(strings) or b'\0' not in strings[name_offset:]:
            raise ValueError('Invalid NSP filename')
        name = strings[name_offset:].split(b'\0', 1)[0].decode('utf-8')
        if '/' in name or '\\' in name or name in {'.', '..'} or base + offset + size > total:
            raise ValueError('Unsafe NSP entry')
        result.append((name, base + offset, size))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nsp', required=True, type=Path)
    parser.add_argument('--keys', required=True, type=Path, help='Private prod.keys or ZIP containing prod.keys')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--hactool', type=Path, default=ROOT / 'tools/hactool/hactool')
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('Choose a new output to preserve prior data')
    original = args.nsp.stat()
    with args.nsp.open('rb') as stream:
        records = entries(stream, original.st_size)
    programs = [r for r in records if r[0].endswith('.nca') and not r[0].endswith('.cnmt.nca')]
    tickets = [r for r in records if r[0].endswith('.tik')]
    if not programs or len(tickets) != 1 or tickets[0][2] < 0x190:
        raise ValueError('Expected a base game with one ticket and program NCA')
    program = max(programs, key=lambda r: r[2])
    args.output.mkdir(parents=True, mode=0o700)
    staging = args.output / '.staging'
    staging.mkdir(mode=0o700)
    key_path = staging / 'prod.keys'
    if zipfile.is_zipfile(args.keys):
        with zipfile.ZipFile(args.keys) as archive:
            candidates = [n for n in archive.namelist() if Path(n).name == 'prod.keys']
            if len(candidates) != 1:
                raise ValueError('Expected one prod.keys in the private archive')
            key_text = archive.read(candidates[0]).decode('utf-8')
    else:
        key_text = args.keys.read_text()
    # Same key-length filtering as the upstream launcher, without printing keys.
    clean = []
    for line in key_text.splitlines():
        match = re.fullmatch(r'\s*([a-zA-Z0-9_]+)\s*=\s*([a-fA-F0-9]+)\s*', line)
        if match and len(match[2]) % 32 == 0:
            clean.append(match[1] + ' = ' + match[2])
    descriptor = os.open(key_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, 'w') as stream:
        stream.write('\n'.join(clean) + '\n')
    nca = staging / 'program.nca'
    with args.nsp.open('rb') as source, nca.open('xb') as target:
        source.seek(program[1])
        remaining = program[2]
        while remaining:
            block = source.read(min(remaining, 4 * 1024 * 1024))
            if not block:
                raise ValueError('Truncated NSP program')
            target.write(block)
            remaining -= len(block)
        source.seek(tickets[0][1] + 0x180)
        encrypted_title_key = source.read(16).hex()
    now = args.nsp.stat()
    if now.st_size != original.st_size or now.st_mtime_ns != original.st_mtime_ns:
        raise ValueError('Dump changed while reading; wait for writing to finish')
    log_path = args.output / 'private-extraction.log'
    descriptor = os.open(log_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, 'wb') as log:
        command = [str(args.hactool.resolve()), '-k', str(key_path.resolve()),
            '--exefsdir=' + str((args.output / 'exefs').resolve()),
            '--romfsdir=' + str((args.output / 'romfs').resolve()),
            '--romfs=' + str((args.output / 'romfs.bin').resolve()),
            '--titlekey=' + encrypted_title_key, str(nca.resolve())]
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError('Extraction failed; inspect the private extraction log without sharing key values')
    with (args.output / 'exefs/main').open('rb') as stream:
        main_header = stream.read(0x100)
    if main_header[:4] != b'NSO0' or main_header[0x40:0x48] != EXPECTED_BUILD:
        raise ValueError("Extracted main is not Link's Awakening v1.0.0")
    if not (args.output / 'exefs/sdk').is_file() or not (args.output / 'romfs.bin').is_file():
        raise ValueError('Extracted game content is incomplete')
    receipt = {'title_id': '01006BB00C6F0000', 'version': '1.0.0',
        'main_build_id': main_header[0x40:0x60].hex(), 'dump_bytes': original.st_size,
        'romfs_bytes': (args.output / 'romfs.bin').stat().st_size,
        'romfs_files': sum(p.is_file() for p in (args.output / 'romfs').rglob('*'))}
    for name in ('main', 'sdk'):
        with (args.output / 'exefs' / name).open('rb') as stream:
            receipt[name + '_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    (args.output / 'extraction-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    shutil.rmtree(staging)
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
