#!/usr/bin/env python3
"""Link locally generated game code as a private module. Never publish its output."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def symbol_names(tool, path, *flags):
    text = subprocess.check_output([tool, *flags, str(path)], text=True)
    return {line.split()[-1].split('@')[0] for line in text.splitlines() if line.strip() and not line.endswith(':')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--linker', default='ld.lld')
    parser.add_argument('--nm', default='llvm-nm')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not args.archive.is_file():
        parser.error(f'Missing generated archive: {args.archive}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location('module_support', ROOT / 'ps5/tools/module-support.py')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    source = args.output.parent / 'module-abi.c'
    obj = source.with_suffix('.o')
    source.write_text(f'__attribute__((visibility("default"))) const char la_game_abi[] = "{support.abi()}";\n')
    subprocess.run([args.clang, '-fPIC', '-fvisibility=default', '-fno-stack-protector', '-c', source, '-o', obj], check=True)
    extra_objects = []
    archive_imports = symbol_names(args.nm, args.archive, '--undefined-only')
    if archive_imports & {'fegetround', 'fesetround'}:
        fenv = args.output.parent / 'module-fenv.o'
        subprocess.run([args.clang, '-fPIC', '-O2', '-fno-stack-protector', '-c',
                        ROOT / 'ps5/module/fenv-x86.c', '-o', fenv], check=True)
        extra_objects.append(fenv)
    subprocess.run([args.linker, '-shared', '-z', 'max-page-size=16384', '-z', 'common-page-size=16384',
                    '--hash-style=sysv', '--whole-archive', args.archive, '--no-whole-archive', obj, *extra_objects,
                    '-o', args.output], check=True)
    exports = symbol_names(args.nm, args.output, '-D', '--defined-only')
    required = {'la_game_abi', 'main_fn_addrs', 'main_fn_ptrs', 'main_fn_count',
                'sdk_fn_addrs', 'sdk_fn_ptrs', 'sdk_fn_count'}
    if required - exports:
        raise ValueError('Missing game module exports: ' + ', '.join(sorted(required - exports)))
    undefined = symbol_names(args.nm, args.output, '-D', '--undefined-only')
    expected = set(support.imports())
    if undefined != expected:
        raise ValueError('Game module import contract differs: missing=' + str(sorted(expected - undefined)) +
                         ' unexpected=' + str(sorted(undefined - expected)))
    if args.output.stat().st_size > 256 * 1024 * 1024:
        raise ValueError('Game module exceeds the loader size limit')
    print(f'Private game module: {args.output} ({args.output.stat().st_size} bytes, {len(undefined)} runtime imports)')


if __name__ == '__main__':
    main()
