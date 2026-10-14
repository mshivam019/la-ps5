#!/usr/bin/env python3
"""Exercise the module loader under ASan/UBSan with synthetic, non-game code."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    (work / 'test.c').write_text(
        'extern void host_throw(void); void unwind_test(void){host_throw();}'
        'extern int host_twice(int); int value=7; int* pointer=&value;'
        '__attribute__((weak)) int weak_value(void){return -1;}'
        'int run(int n){return host_twice(n)+*pointer+weak_value();}\n')

    def run(*args):
        environment = os.environ.copy()
        environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
        subprocess.run(list(map(str, args)), check=True, env=environment)

    run('clang', '-fexceptions', '-funwind-tables', '-fPIC', '-fno-stack-protector', '-c',
        work / 'test.c', '-o', work / 'test.o')
    run('ld.lld', '-shared', '-z', 'max-page-size=16384', '-z', 'common-page-size=16384',
        work / 'test.o', '-o', work / 'test.so')
    data = (work / 'test.so').read_bytes()
    invalid = []
    for name, content in [('truncated', data[:20]), ('wrong-machine', data[:18] + b'\xb7\x00' + data[20:]),
                          ('bad-phoff', data[:32] + struct.pack('<Q', 2**63) + data[40:]), ('empty', b'')]:
        path = work / name
        path.write_bytes(content)
        invalid.append(path)
    section_offset = struct.unpack_from('<Q', data, 40)[0]
    section_size, section_count = struct.unpack_from('<HH', data, 58)
    relocation = next(struct.unpack_from('<Q', data, section_offset + i * section_size + 24)[0]
                      for i in range(section_count)
                      if struct.unpack_from('<I', data, section_offset + i * section_size + 4)[0] == 4)
    for name, offset, value in [('bad-relocation-target', relocation, 2**63),
                                ('unsupported-relocation', relocation + 8, 255)]:
        changed = bytearray(data)
        struct.pack_into('<Q', changed, offset, value)
        path = work / name
        path.write_bytes(changed)
        invalid.append(path)
    run('clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g',
        '-I' + str(ROOT / 'ps5/module'), ROOT / 'ps5/tests/module-loader.cpp',
        ROOT / 'ps5/module/loader.cpp', '-o', work / 'test')
    run(work / 'test', work / 'test.so', *invalid, work / 'nonexistent')
