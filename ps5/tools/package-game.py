#!/usr/bin/env python3
"""Prepare private user-owned game files for the separately installed ROM-free app."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

TITLE = 'PPSA99670'
BUILD_ID = bytes.fromhex('ae16f71e002af8cb')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path, help='Private extracted Link’s Awakening v1.0.0 folder')
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='A new folder whose name is PPSA99670')
    args = parser.parse_args()
    game = args.game.resolve()
    if args.output.exists():
        parser.error('Choose a new output folder to preserve existing saves and settings')
    if args.output.name != TITLE:
        parser.error(f'--output must end in {TITLE}')
    with (game / 'exefs/main').open('rb') as stream:
        header = stream.read(0x100)
    if header[:4] != b'NSO0' or header[0x40:0x48] != BUILD_ID:
        parser.error("Only an owned Link’s Awakening v1.0.0 base-game dump is supported; updates are unsupported")
    if not (game / 'exefs/sdk').is_file() or not (game / 'romfs/region_common').is_dir() or not (game / 'romfs.bin').is_file():
        parser.error('The extracted ExeFS, raw RomFS, and extracted RomFS are all required')
    if not args.module.is_file():
        parser.error('Private game module is missing')
    private = args.output / 'extracted'
    private.mkdir(parents=True)
    for directory in ('exefs', 'romfs'):
        source = game / directory
        if source.is_symlink():
            parser.error(f'Private input contains a symlink: {source}')
        for path in source.rglob('*'):
            if path.is_symlink():
                parser.error(f'Private input contains a symlink: {path}')
        shutil.copytree(source, private / directory)
    shutil.copy2(game / 'romfs.bin', private / 'romfs.bin')
    shutil.copy2(args.module, private / 'game.la')
    files = [{'path': path.relative_to(args.output).as_posix(), 'size': path.stat().st_size, 'sha256': sha(path)}
             for path in sorted(args.output.rglob('*')) if path.is_file()]
    (args.output / 'private-game-manifest.json').write_text(json.dumps({
        'title_id': TITLE, 'private_game_data': True, 'game_version': '1.0.0', 'files': files
    }, indent=2) + '\n')
    (args.output / 'COPY-TO-PS5.txt').write_text(
        'Install the matching ROM-free Link’s Awakening app first.\n'
        'Close it, then copy extracted/ into its PPSA99670 folder.\n'
        'Keep user/ so saves, settings, controls, and shader caches remain intact.\n'
        'This package contains your game data and generated game.la; do not redistribute it.\n')
    print(f'Private game package ready: {args.output}')


if __name__ == '__main__':
    main()
