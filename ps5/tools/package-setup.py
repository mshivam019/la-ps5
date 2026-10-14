#!/usr/bin/env python3
"""Package tracked source and the licensed host extractor; never include game data."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    if a.output.exists(): p.error('Output exists')
    if subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain']).strip():
        p.error('Commit the source before packaging')
    names = subprocess.check_output(['git', '-C', ROOT, 'ls-files', '-z']).decode().split('\0')
    files = {name: ROOT / name for name in names if name}
    files['host-tools/linux-x86_64/hactool'] = ROOT / 'tools/hactool/hactool'
    banned = {'generated', 'extracted', 'private-assets', 'save', 'cache', 'build', 'dist', '.git', '.deps'}
    manifest = []
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(a.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, path in sorted(files.items()):
            relative = Path(name)
            if path.is_symlink() or any(x in banned for x in relative.parts) or relative.suffix in {'.nsp', '.xci', '.nca', '.nso', '.keys', '.la'}:
                raise ValueError('Private/linked input in source: ' + name)
            data = path.read_bytes()
            z.write(path, name)
            manifest.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        z.writestr('setup-manifest.json', json.dumps({'files': manifest}, indent=2) + '\n')
    with zipfile.ZipFile(a.output) as z:
        if z.testzip(): raise ValueError('ZIP CRC mismatch')
        for row in manifest:
            data = z.read(row['path'])
            if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
                raise ValueError('ZIP mismatch: ' + row['path'])
    with a.output.open('rb') as f: digest = hashlib.file_digest(f, 'sha256').hexdigest()
    a.output.with_suffix('.zip.sha256').write_text(digest + '  ' + a.output.name + '\n')
    print(f'{a.output}: {len(files)} verified source/setup files')
if __name__ == '__main__': main()
