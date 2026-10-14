#!/usr/bin/env python3
"""Prepare pinned development tools and Vulkan headers; never read game keys."""
import hashlib
import json
from pathlib import Path
import subprocess
import shutil
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def main():
    pins = json.loads((ROOT / 'ps5/dependencies.json').read_text())
    downloads = ROOT / '.deps/downloads'
    downloads.mkdir(parents=True, exist_ok=True)
    for item in pins['archives']:
        archive = downloads / item['filename']
        if not archive.exists():
            partial = archive.with_suffix(archive.suffix + '.part')
            urllib.request.urlretrieve(item['url'], partial)
            partial.rename(archive)
        with archive.open('rb') as stream:
            sha = hashlib.file_digest(stream, 'sha256').hexdigest()
        if sha != item['sha256']:
            raise ValueError(f'Dependency SHA256 mismatch: {archive.name}')
        output = ROOT / '.deps/src' / item['source_directory']
        if not output.exists():
            output.mkdir(parents=True)
            with tarfile.open(archive) as source:
                for member in source.getmembers():
                    member.name = '/'.join(member.name.split('/')[1:])
                    if member.name:
                        source.extract(member, output, filter='data')
        print('Verified', archive.name)
    venv = ROOT / 'tools/venv'
    if not venv.exists():
        subprocess.run([sys.executable, '-m', 'venv', str(venv)], check=True)
    python = venv / 'bin/python'
    requirements = [f'{name}=={version}' for name, version in pins['python'].items()]
    subprocess.run([str(python), '-m', 'pip', 'install', *requirements], check=True)
    hactool = ROOT / 'tools/hactool'
    if not hactool.exists():
        subprocess.run(['git', 'clone', pins['hactool']['url'], str(hactool)], check=True)
        subprocess.run(['git', '-C', str(hactool), 'checkout', '--detach', pins['hactool']['revision']], check=True)
    revision = subprocess.check_output(['git', '-C', str(hactool), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != pins['hactool']['revision']:
        raise ValueError('Preserving hactool at a different revision; review it before extraction')
    config = hactool / 'config.mk'
    if not config.exists():
        shutil.copy2(hactool / 'config.mk.template', config)
    subprocess.run(['make', '-C', str(hactool), 'CC=clang'], check=True)
    print('Use the existing native SDK, native SDL3 and RADV workspace recorded in dependencies.json.')


if __name__ == '__main__':
    main()
