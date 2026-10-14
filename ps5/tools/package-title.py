#!/usr/bin/env python3
"""Package the ROM-free PS5 application as a catalog-compatible title ZIP."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TITLE = 'PPSA99670'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New staging folder ending in PPSA99670')
    parser.add_argument('--zip', dest='archive', type=Path, required=True)
    args = parser.parse_args()
    if args.output.name != TITLE or args.output.exists() or args.archive.exists():
        parser.error('Choose a new PPSA99670 folder and ZIP path')
    subprocess.run(['python3', ROOT / 'ps5/tools/validate.py', '--title'], check=True, stdout=subprocess.DEVNULL)
    param = json.loads((ROOT / 'ps5/sce_sys/param.json').read_text())
    if param['titleId'] != TITLE:
        raise ValueError('Unexpected title ID')
    files = {
        'eboot.bin': ROOT / 'build/native-title/eboot.bin',
        'sce_module/libc.prx': ROOT.parent / 'PS5_Vulkan/runtime/libc.prx',
        'Baloo2-SemiBold.ttf': ROOT / 'third_party/fonts/Baloo2/Baloo2-SemiBold.ttf',
        'DejaVuSans.ttf': Path('/usr/share/fonts/TTF/DejaVuSans.ttf'),
        'licenses/LAS-MIT.txt': ROOT / 'LICENSE',
        'NOTICE': ROOT / 'NOTICE',
        'THIRD_PARTY_NOTICES.txt': ROOT / 'ps5/THIRD_PARTY_NOTICES.txt',
        'ARTWORK.md': ROOT / 'ps5/ARTWORK.md',
        'SETUP.md': ROOT / 'ps5/README.md',
    }
    for directory, prefix in [('ps5/sce_sys','sce_sys'), ('ps5/licenses','licenses')]:
        for path in (ROOT / directory).iterdir():
            if path.is_file():
                files[prefix + '/' + path.name] = path
    expected_libc = (ROOT.parent / 'PS5_Vulkan/runtime/libc.prx.sha256').read_text().split()[0]
    if digest(files['sce_module/libc.prx']) != expected_libc:
        raise ValueError('Runtime libc provenance mismatch')
    manifest = {'title_id': TITLE, 'content_version': param['contentVersion'], 'rom_free': True, 'files': []}
    for name, source in sorted(files.items()):
        if source.is_symlink() or not source.is_file():
            raise ValueError(f'Invalid package input: {name}')
        target = args.output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        sha = digest(source)
        if digest(target) != sha:
            raise ValueError(f'Copy mismatch: {name}')
        manifest['files'].append({'path': name, 'bytes': target.stat().st_size, 'sha256': sha})
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    args.archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(args.output.rglob('*')):
            if path.is_file():
                archive.write(path, TITLE + '/' + path.relative_to(args.output).as_posix())
    with zipfile.ZipFile(args.archive) as archive:
        if archive.testzip():
            raise ValueError('ZIP CRC mismatch')
        for row in manifest['files']:
            data = archive.read(TITLE + '/' + row['path'])
            if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
                raise ValueError(f'ZIP content mismatch: {row["path"]}')
    args.archive.with_suffix('.zip.sha256').write_text(digest(args.archive) + '  ' + args.archive.name + '\n')
    print(f'{args.archive}: {len(files)} verified ROM-free application files')


if __name__ == '__main__':
    main()
