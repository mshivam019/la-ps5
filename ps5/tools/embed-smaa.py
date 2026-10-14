#!/usr/bin/env python3
"""Embed the upstream SMAA shader with the same ASCII normalization as Makefile."""
import json
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes().replace(b'\r', b'')
text = ''.join(chr(byte) if byte < 128 else '?' for byte in source)
Path(sys.argv[2]).write_text('\n'.join(json.dumps(line) for line in text.splitlines(keepends=True)) + '\n')
