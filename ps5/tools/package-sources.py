#!/usr/bin/env python3
"""Archive the exact local dependency source trees used by this workspace build."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output exists; choose a new artifact')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    workspace = ROOT.parent
    repositories = {
        'hactool': ROOT / 'tools/hactool',
        'sdk-platform': workspace / 'PS5_PayloadSDK',
        'vulkan': workspace / 'PS5_Vulkan',
        'mesa': workspace / 'Dusklight-PS5/.deps/src/mesa',
        'SDL': workspace / 'Dusklight-PS5/.deps/src/SDL',
        'native-app-boilerplate': Path.home() / 'ps5sdk/native-app-boilerplate',
    }
    directories = {name: ROOT / '.deps/src' / name for name in
                   ('ffmpeg', 'shaderc', 'VulkanMemoryAllocator')}
    directories['host-zlib'] = workspace / 'PS5_Vulkan/.deps/native/zlib/zlib-1.3.2'
    # Keep source only, including the locally patched shaderc tree. Generated
    # game code is outside this explicit allowlist and can never be traversed.
    excluded = {'.git', '__pycache__', '.cache', 'build', 'CMakeFiles'}
    def source_filter(member):
        parts = Path(member.name).parts
        if any(part in excluded for part in parts):
            return None
        if member.name.endswith(('.tar.xz', '.tar.gz', '.o', '.a', '.so', '.pyc')):
            return None
        return member
    manifest = {'repositories': {}, 'files': []}
    temporary = args.output.with_suffix(args.output.suffix + '.part')
    try:
        with tarfile.open(temporary, 'w:gz') as output:
            for name, repo in repositories.items():
                revision = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
                changes = subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--untracked-files=no'], text=True)
                manifest['repositories'][name] = {'revision': revision, 'modified': bool(changes)}
                changed = set(filter(None, subprocess.check_output(['git', '-C', str(repo), 'diff', '--name-only', '-z', 'HEAD']).decode().split('\0')))
                with tempfile.TemporaryFile() as stream:
                    subprocess.run(['git', '-C', str(repo), 'archive', '--format=tar', '--prefix=' + name + '/', revision], stdout=stream, check=True)
                    stream.seek(0)
                    with tarfile.open(fileobj=stream) as source:
                        for member in source:
                            if member.name.removeprefix(name + '/') in changed:
                                continue
                            output.addfile(member, source.extractfile(member) if member.isfile() else None)
                if changes:
                    patch = subprocess.check_output(['git', '-C', str(repo), 'diff', '--binary', 'HEAD'])
                    info = tarfile.TarInfo(name + '/LOCAL-BUILD.patch')
                    info.size = len(patch)
                    output.addfile(info, io.BytesIO(patch))
                    # Replace modified tracked files with their actual build input.
                    for relative in filter(None, changed):
                        path = repo / relative
                        if path.is_file():
                            output.add(path, arcname=name + '/' + relative)
            for name, directory in directories.items():
                if not directory.is_dir():
                    raise ValueError(f'Missing source tree: {directory}')
                output.add(directory, arcname=name, filter=source_filter)
            output.add(ROOT / 'ps5/dependencies.json', arcname='build-dependencies.json')
            output.add(workspace / 'PS5_Vulkan/.deps/native/radv-release/PROVENANCE.txt', arcname='RADV-PROVENANCE.txt')
        # Independently enumerate and hash every archived file after compression.
        with tarfile.open(temporary) as archive:
            for member in archive:
                if member.isfile():
                    manifest['files'].append({'path': member.name, 'bytes': member.size,
                        'sha256': hashlib.file_digest(archive.extractfile(member), 'sha256').hexdigest()})
        temporary.rename(args.output)
        with args.output.open('rb') as stream:
            manifest['sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
        args.output.with_suffix(args.output.suffix + '.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'{args.output}: {len(manifest["files"])} verified source files; SHA256 {manifest["sha256"]}')
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
