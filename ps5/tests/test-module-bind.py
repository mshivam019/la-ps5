#!/usr/bin/env python3
"""Bind the real private module and reject incompatible synthetic interfaces."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / 'build/private-game-module/game.la'
if not MODULE.is_file():
    raise SystemExit('build the private test module first')

spec = importlib.util.spec_from_file_location('module_support', ROOT / 'ps5/tools/module-support.py')
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)

with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    support.generate(work / 'generated')
    source = '''
#include <stdint.h>
typedef void (*Fn)(void*);
static void f(void* p) {(void)p;}
const char la_game_abi[65] = ABI;
const uint64_t main_fn_addrs[] = {0x7000000000ull};
const Fn main_fn_ptrs[] = {f};
const uint64_t main_fn_count = 1;
SDK
'''
    sdk = '''const uint64_t sdk_fn_addrs[] = {0x7004000000ull};
const Fn sdk_fn_ptrs[] = {f};
const uint64_t sdk_fn_count = 1;'''
    cases = [('wrong.so', source.replace('ABI', '"wrong"').replace('SDK', sdk), 'different app version'),
             ('missing.so', source.replace('ABI', '"' + support.abi() + '"').replace('SDK', ''),
              'Missing game module symbol')]
    for name, text, _ in cases:
        path = work / (name + '.c')
        path.write_text(text)
        obj = work / (name + '.o')
        subprocess.run(['clang', '-fPIC', '-fno-stack-protector', '-c', path, '-o', obj], check=True)
        subprocess.run(['ld.lld', '-shared', '-z', 'max-page-size=16384', '-z', 'common-page-size=16384',
                        obj, '-o', work / name], check=True)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I' + str(ROOT / 'ps5/module'), '-I' + str(ROOT / 'runtime'), '-I' + str(work / 'generated'),
                    ROOT / 'ps5/tests/module-bind.cpp', ROOT / 'ps5/module/bind.cpp', ROOT / 'ps5/module/loader.cpp',
                    '-o', work / 'bind'], check=True)
    environment = os.environ.copy()
    environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    subprocess.run([work / 'bind', MODULE, 'ok'], check=True, env=environment)
    for name, _, error in cases:
        subprocess.run([work / 'bind', work / name, error], check=True, env=environment)
    print('Real module binding and incompatible interface rejection passed')
