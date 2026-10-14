#!/usr/bin/env python3
"""Private packaging includes only the selected dump/module and rejects update/wrong inputs."""
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    game = work / 'game'
    (game / 'exefs').mkdir(parents=True)
    (game / 'romfs/region_common').mkdir(parents=True)
    header = bytearray(0x100)
    header[:4] = b'NSO0'
    header[0x40:0x48] = bytes.fromhex('ae16f71e002af8cb')
    (game / 'exefs/main').write_bytes(header)
    (game / 'exefs/sdk').write_bytes(header)
    (game / 'romfs/region_common/file').write_bytes(b'user game data')
    (game / 'romfs.bin').write_bytes(b'raw romfs')
    module = work / 'game.la'
    module.write_bytes(b'private generated code')
    keys = game / 'prod.keys'
    keys.write_text('secret = must-not-copy\n')
    output = work / 'PPSA99670'
    subprocess.run(['python3', ROOT / 'ps5/tools/package-game.py', '--game', game,
                    '--module', module, '--output', output], check=True)
    assert (output / 'extracted/game.la').read_bytes() == module.read_bytes()
    assert not any(path.name == 'prod.keys' for path in output.rglob('*'))
    assert (output / 'extracted/romfs/region_common/file').is_file()
    wrong = bytearray(header)
    wrong[0x40] ^= 1
    (game / 'exefs/main').write_bytes(wrong)
    failed = subprocess.run(['python3', ROOT / 'ps5/tools/package-game.py', '--game', game,
                             '--module', module, '--output', work / 'wrong/PPSA99670'])
    assert failed.returncode != 0
    print('Private packaging and wrong-version rejection passed')
